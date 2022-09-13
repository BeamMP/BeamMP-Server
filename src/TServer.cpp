#include "TServer.h"
#include "Client.h"
#include "Common.h"
#include "TNetwork.h"
#include "TPPSMonitor.h"
#include <TLuaPlugin.h>
#include <any>
#include <sstream>

#include <nlohmann/json.hpp>

#include "LuaAPI.h"

#undef GetObject // Fixes Windows

#include "Json.h"

static std::optional<std::pair<int, int>> GetPidVid(std::string str) {
    auto IDSep = str.find('-');
    std::string pid = str.substr(0, IDSep);
    std::string vid = str.substr(IDSep + 1);

    if (pid.find_first_not_of("0123456789") == std::string::npos && vid.find_first_not_of("0123456789") == std::string::npos) {
        try {
        int PID = stoi(pid);
        int VID = stoi(vid);
        return {{ PID, VID }};
        } catch(const std::exception&) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

TEST_CASE("GetPidVid") {
    SUBCASE("Valid singledigit") {
        const auto MaybePidVid = GetPidVid("0-1");
        CHECK(MaybePidVid);
        auto [pid, vid] = MaybePidVid.value();

        CHECK_EQ(pid, 0);
        CHECK_EQ(vid, 1);
    }
    SUBCASE("Valid doubledigit") {
        const auto MaybePidVid = GetPidVid("10-12");
        CHECK(MaybePidVid);
        auto [pid, vid] = MaybePidVid.value();

        CHECK_EQ(pid, 10);
        CHECK_EQ(vid, 12);
    }
    SUBCASE("Empty string") {
        const auto MaybePidVid = GetPidVid("");
        CHECK(!MaybePidVid);
    }
    SUBCASE("Invalid separator") {
        const auto MaybePidVid = GetPidVid("0x0");
        CHECK(!MaybePidVid);
    }
    SUBCASE("Missing pid") {
        const auto MaybePidVid = GetPidVid("-0");
        CHECK(!MaybePidVid);
    }
    SUBCASE("Missing vid") {
        const auto MaybePidVid = GetPidVid("0-");
        CHECK(!MaybePidVid);
    }
    SUBCASE("Invalid pid") {
        const auto MaybePidVid = GetPidVid("x-0");
        CHECK(!MaybePidVid);
    }
    SUBCASE("Invalid vid") {
        const auto MaybePidVid = GetPidVid("0-x");
        CHECK(!MaybePidVid);
    }
}


TServer::TServer(const std::vector<std::string_view>& Arguments) {
    beammp_info("BeamMP Server v" + Application::ServerVersionString());
    Application::SetSubsystemStatus("Server", Application::Status::Starting);
    if (Arguments.size() > 1) {
        Application::Settings.CustomIP = Arguments[0];
        size_t n = std::count(Application::Settings.CustomIP.begin(), Application::Settings.CustomIP.end(), '.');
        auto p = Application::Settings.CustomIP.find_first_not_of(".0123456789");
        if (p != std::string::npos || n != 3 || Application::Settings.CustomIP.substr(0, 3) == "127") {
            Application::Settings.CustomIP.clear();
            beammp_warn("IP Specified is invalid! Ignoring");
        } else {
            beammp_info("server started with custom IP");
        }
    }
    Application::SetSubsystemStatus("Server", Application::Status::Good);
}

void TServer::RemoveClient(const std::weak_ptr<TClient>& WeakClientPtr) {
    if (!WeakClientPtr.expired()) {
        TClient& Client = *WeakClientPtr.lock();
        beammp_debug("removing client " + Client.GetName() + " (" + std::to_string(ClientCount()) + ")");
        Client.ClearCars();
        WriteLock Lock(mClientsMutex);
        mClients.erase(WeakClientPtr.lock());
    }
}

std::weak_ptr<TClient> TServer::InsertNewClient() {
    beammp_debug("inserting new client (" + std::to_string(ClientCount()) + ")");
    WriteLock Lock(mClientsMutex);
    auto [Iter, Replaced] = mClients.insert(std::make_shared<TClient>(*this));
    return *Iter;
}

void TServer::ForEachClient(const std::function<bool(std::weak_ptr<TClient>)>& Fn) {
    decltype(mClients) Clients;
    {
        ReadLock lock(mClientsMutex);
        Clients = mClients;
    }
    for (auto& Client : Clients) {
        if (!Fn(Client)) {
            break;
        }
    }
}

size_t TServer::ClientCount() const {
    ReadLock Lock(mClientsMutex);
    return mClients.size();
}

void TServer::GlobalParser(const std::weak_ptr<TClient>& Client, std::string Packet, TPPSMonitor& PPSMonitor, TNetwork& Network) {
    if (Packet.find("Zp") != std::string::npos && Packet.size() > 500) {
        // abort();
    }
    if (Packet.substr(0, 4) == "ABG:") {
        Packet = DeComp(Packet.substr(4));
    }
    if (Packet.empty()) {
        return;
    }

    if (Client.expired()) {
        return;
    }
    auto LockedClient = Client.lock();

    std::any Res;
    char Code = Packet.at(0);

    // V to Y
    if (Code <= 89 && Code >= 86) {
        PPSMonitor.IncrementInternalPPS();
        Network.SendToAll(LockedClient.get(), Packet, false, false);
        return;
    }
    switch (Code) {
    case 'H': // initial connection
        beammp_trace(std::string("got 'H' packet: '") + Packet + "' (" + std::to_string(Packet.size()) + ")");
        if (!Network.SyncClient(Client)) {
            // TODO handle
        }
        return;
    case 'p':
        if (!Network.Respond(*LockedClient, ("p"), false)) {
            // failed to send
            if (LockedClient->GetStatus() > -1) {
                LockedClient->SetStatus(-1);
            }
        } else {
            Network.UpdatePlayer(*LockedClient);
        }
        return;
    case 'O':
        if (Packet.length() > 1000) {
            beammp_debug(("Received data from: ") + LockedClient->GetName() + (" Size: ") + std::to_string(Packet.length()));
        }
        ParseVehicle(*LockedClient, Packet, Network);
        return;
    case 'J':
        beammp_trace(std::string(("got 'J' packet: '")) + Packet + ("' (") + std::to_string(Packet.size()) + (")"));
        Network.SendToAll(LockedClient.get(), Packet, false, true);
        return;
    case 'C': {
        beammp_trace(std::string(("got 'C' packet: '")) + Packet + ("' (") + std::to_string(Packet.size()) + (")"));
        if (Packet.length() < 4 || Packet.find(':', 3) == std::string::npos)
            break;
        auto Futures = LuaAPI::MP::Engine->TriggerEvent("onChatMessage", "", LockedClient->GetID(), LockedClient->GetName(), Packet.substr(Packet.find(':', 3) + 2));
        TLuaEngine::WaitForAll(Futures);
        LogChatMessage(LockedClient->GetName(), LockedClient->GetID(), Packet.substr(Packet.find(':', 3) + 1));
        if (std::any_of(Futures.begin(), Futures.end(),
                [](const std::shared_ptr<TLuaResult>& Elem) {
                    return !Elem->Error
                        && Elem->Result.is<int>()
                        && bool(Elem->Result.as<int>());
                })) {
            break;
        }
        Network.SendToAll(nullptr, Packet, true, true);
        return;
    }
    case 'E':
        beammp_trace(std::string(("got 'E' packet: '")) + Packet + ("' (") + std::to_string(Packet.size()) + (")"));
        HandleEvent(*LockedClient, Packet);
        return;
    case 'N':
        beammp_trace("got 'N' packet (" + std::to_string(Packet.size()) + ")");
        Network.SendToAll(LockedClient.get(), Packet, false, true);
        return;
    case 'Z': // position packet
        PPSMonitor.IncrementInternalPPS();
        Network.SendToAll(LockedClient.get(), Packet, false, false);

        HandlePosition(*LockedClient, Packet);
    default:
        return;
    }
}

void TServer::HandleEvent(TClient& c, const std::string& RawData) {
    // E:Name:Data
    // Data is allowed to have ':'
    auto NameDataSep = RawData.find(':', 2);
    if (NameDataSep == std::string::npos) {
        beammp_warn("received event in invalid format (missing ':'), got: '" + RawData + "'");
    }
    std::string Name = RawData.substr(2, NameDataSep - 2);
    std::string Data = RawData.substr(NameDataSep + 1);
    LuaAPI::MP::Engine->ReportErrors(LuaAPI::MP::Engine->TriggerEvent(Name, "", c.GetID(), Data));
}

bool TServer::IsUnicycle(TClient& c, const std::string& CarJson) {
    try {
        auto Car = nlohmann::json::parse(CarJson);
        const std::string jbm = "jbm";
        if (Car.contains(jbm) && Car[jbm].is_string() && Car[jbm] == "unicycle") {
            return true;
        }
    } catch (const std::exception& e) {
        beammp_error("Failed to parse vehicle data as json for client " + std::to_string(c.GetID()) + ": '" + CarJson + "'");
    }
    return false;
}

bool TServer::ShouldSpawn(TClient& c, const std::string& CarJson, int ID) {
    if (IsUnicycle(c, CarJson) && c.GetUnicycleID() < 0) {
        c.SetUnicycleID(ID);
        return true;
    } else {
        return c.GetCarCount() < Application::Settings.MaxCars;
    }
}

void TServer::ParseVehicle(TClient& c, const std::string& Pckt, TNetwork& Network) {
    if (Pckt.length() < 4)
        return;
    std::string Packet = Pckt;
    char Code = Packet.at(1);
    int PID = -1;
    int VID = -1, Pos;
    std::string Data = Packet.substr(3), pid, vid;
    switch (Code) { // Spawned Destroyed Switched/Moved NotFound Reset
    case 's':
        beammp_trace(std::string(("got 'Os' packet: '")) + Packet + ("' (") + std::to_string(Packet.size()) + (")"));
        if (Data.at(0) == '0') {
            int CarID = c.GetOpenCarID();
            beammp_debug(c.GetName() + (" created a car with ID ") + std::to_string(CarID));

            std::string CarJson = Packet.substr(5);
            Packet = "Os:" + c.GetRoles() + ":" + c.GetName() + ":" + std::to_string(c.GetID()) + "-" + std::to_string(CarID) + ":" + CarJson;
            auto Futures = LuaAPI::MP::Engine->TriggerEvent("onVehicleSpawn", "", c.GetID(), CarID, Packet.substr(3));
            TLuaEngine::WaitForAll(Futures);
            bool ShouldntSpawn = std::any_of(Futures.begin(), Futures.end(),
                [](const std::shared_ptr<TLuaResult>& Result) {
                    return !Result->Error && Result->Result.is<int>() && Result->Result.as<int>() != 0;
                });

            if (ShouldSpawn(c, CarJson, CarID) && !ShouldntSpawn) {
                c.AddNewCar(CarID, Packet);
                Network.SendToAll(nullptr, Packet, true, true);
            } else {
                if (!Network.Respond(c, Packet, true)) {
                    // TODO: handle
                }
                std::string Destroy = "Od:" + std::to_string(c.GetID()) + "-" + std::to_string(CarID);
                if (!Network.Respond(c, Destroy, true)) {
                    // TODO: handle
                }
                beammp_debug(c.GetName() + (" (force : car limit/lua) removed ID ") + std::to_string(CarID));
            }
        }
        return;
    case 'c': {
        beammp_trace(std::string(("got 'Oc' packet: '")) + Packet + ("' (") + std::to_string(Packet.size()) + (")"));
        auto MaybePidVid = GetPidVid(Data.substr(0, Data.find(':', 1)));
        if (MaybePidVid) {
            std::tie(PID, VID) = MaybePidVid.value();
        }
        if (PID != -1 && VID != -1 && PID == c.GetID()) {
            auto Futures = LuaAPI::MP::Engine->TriggerEvent("onVehicleEdited", "", c.GetID(), VID, Packet.substr(3));
            TLuaEngine::WaitForAll(Futures);
            bool ShouldntAllow = std::any_of(Futures.begin(), Futures.end(),
                [](const std::shared_ptr<TLuaResult>& Result) {
                    return !Result->Error && Result->Result.is<int>() && Result->Result.as<int>() != 0;
                });

            auto FoundPos = Packet.find('{');
            FoundPos = FoundPos == std::string::npos ? 0 : FoundPos; // attempt at sanitizing this
            if ((c.GetUnicycleID() != VID || IsUnicycle(c, Packet.substr(FoundPos)))
                && !ShouldntAllow) {
                Network.SendToAll(&c, Packet, false, true);
                Apply(c, VID, Packet);
            } else {
                if (c.GetUnicycleID() == VID) {
                    c.SetUnicycleID(-1);
                }
                std::string Destroy = "Od:" + std::to_string(c.GetID()) + "-" + std::to_string(VID);
                Network.SendToAll(nullptr, Destroy, true, true);
                c.DeleteCar(VID);
            }
        }
        return;
    }
    case 'd': {
        beammp_trace(std::string(("got 'Od' packet: '")) + Packet + ("' (") + std::to_string(Packet.size()) + (")"));
        auto MaybePidVid = GetPidVid(Data);
        if (MaybePidVid) {
            std::tie(PID, VID) = MaybePidVid.value();
        }
        if (PID != -1 && VID != -1 && PID == c.GetID()) {
            if (c.GetUnicycleID() == VID) {
                c.SetUnicycleID(-1);
            }
            Network.SendToAll(nullptr, Packet, true, true);
            // TODO: should this trigger on all vehicle deletions?
            LuaAPI::MP::Engine->ReportErrors(LuaAPI::MP::Engine->TriggerEvent("onVehicleDeleted", "", c.GetID(), VID));
            c.DeleteCar(VID);
            beammp_debug(c.GetName() + (" deleted car with ID ") + std::to_string(VID));
        }
        return;
    }
    case 'r': {
        beammp_trace(std::string(("got 'Or' packet: '")) + Packet + ("' (") + std::to_string(Packet.size()) + (")"));
        auto MaybePidVid = GetPidVid(Data);
        if (MaybePidVid) {
            std::tie(PID, VID) = MaybePidVid.value();
        }

        if (PID != -1 && VID != -1 && PID == c.GetID()) {
            Data = Data.substr(Data.find('{'));
            LuaAPI::MP::Engine->ReportErrors(LuaAPI::MP::Engine->TriggerEvent("onVehicleReset", "", c.GetID(), VID, Data));
            Network.SendToAll(&c, Packet, false, true);
        }
        return;
    }
    case 't':
        beammp_trace(std::string(("got 'Ot' packet: '")) + Packet + ("' (") + std::to_string(Packet.size()) + (")"));
        Network.SendToAll(&c, Packet, false, true);
        return;
    case 'm':
        Network.SendToAll(&c, Packet, true, true);
        return;
    default:
        beammp_trace(std::string(("possibly not implemented: '") + Packet + ("' (") + std::to_string(Packet.size()) + (")")));
        return;
    }
}

void TServer::Apply(TClient& c, int VID, const std::string& pckt) {
    auto FoundPos = pckt.find('{');
    if (FoundPos == std::string::npos) {
        beammp_error("Malformed packet received, no '{' found");
        return;
    }
    std::string Packet = pckt.substr(FoundPos);
    std::string VD = c.GetCarData(VID);
    if (VD.empty()) {
        beammp_error("Tried to apply change to vehicle that does not exist");
        auto Lock = Sentry.CreateExclusiveContext();
        Sentry.SetContext("vehicle-change",
            { { "packet", Packet },
                { "vehicle-id", std::to_string(VID) },
                { "client-car-count", std::to_string(c.GetCarCount()) } });
        Sentry.LogError("attempt to apply change to nonexistent vehicle", _file_basename, _line);
        return;
    }
    std::string Header = VD.substr(0, VD.find('{'));

    FoundPos = VD.find('{');
    if (FoundPos == std::string::npos) {
        auto Lock = Sentry.CreateExclusiveContext();
        Sentry.SetContext("vehicle-change-packet",
            { { "packet", VD } });
        Sentry.LogError("malformed packet", _file_basename, _line);
        return;
    }
    VD = VD.substr(FoundPos);
    rapidjson::Document Veh, Pack;
    Veh.Parse(VD.c_str());
    if (Veh.HasParseError()) {
        beammp_error("Could not get vehicle config!");
        return;
    }
    Pack.Parse(Packet.c_str());
    if (Pack.HasParseError() || Pack.IsNull()) {
        beammp_error("Could not get active vehicle config!");
        return;
    }

    for (auto& M : Pack.GetObject()) {
        if (Veh[M.name].IsNull()) {
            Veh.AddMember(M.name, M.value, Veh.GetAllocator());
        } else {
            Veh[M.name] = Pack[M.name];
        }
    }
    rapidjson::StringBuffer Buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(Buffer);
    Veh.Accept(writer);
    c.SetCarData(VID, Header + Buffer.GetString());
}

void TServer::InsertClient(const std::shared_ptr<TClient>& NewClient) {
    beammp_debug("inserting client (" + std::to_string(ClientCount()) + ")");
    WriteLock Lock(mClientsMutex); // TODO why is there 30+ threads locked here
    (void)mClients.insert(NewClient);
}

void TServer::HandlePosition(TClient& c, std::string Packet) {
    // Zp:serverVehicleID:data
    std::string withoutCode = Packet.substr(3);
    auto NameDataSep = withoutCode.find(':', 2);
    std::string ServerVehicleID = withoutCode.substr(2, NameDataSep - 2);
    std::string Data = withoutCode.substr(NameDataSep + 1);

    // parse veh ID
    auto MaybePidVid = GetPidVid(ServerVehicleID);
    if (MaybePidVid) {
        int PID = -1;
        int VID = -1;
        std::tie(PID, VID) = MaybePidVid.value();

        c.SetCarPosition(VID, Data);
    }
}
