#include "TServer.h"
#include "Client.h"
#include "Common.h"
#include "CustomAssert.h"
#include "TNetwork.h"
#include "TPPSMonitor.h"
#include <TLuaPlugin.h>
#include <algorithm>
#include <any>
#include <optional>
#include <sstream>

#include <nlohmann/json.hpp>

#include "LuaAPI.h"

#undef GetObject // Fixes Windows

#include "Json.h"

static std::optional<std::pair<int, int>> GetPidVid(const std::string& str) {
    auto IDSep = str.find('-');
    std::string pid = str.substr(0, IDSep);
    std::string vid = str.substr(IDSep + 1);

    if (pid.find_first_not_of("0123456789") == std::string::npos && vid.find_first_not_of("0123456789") == std::string::npos) {
        try {
            int PID = stoi(pid);
            int VID = stoi(vid);
            return { { PID, VID } };
        } catch (const std::exception&) {
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
    SUBCASE("Valid doubledigit 2") {
        const auto MaybePidVid = GetPidVid("10-2");
        CHECK(MaybePidVid);
        auto [pid, vid] = MaybePidVid.value();

        CHECK_EQ(pid, 10);
        CHECK_EQ(vid, 2);
    }
    SUBCASE("Valid doubledigit 3") {
        const auto MaybePidVid = GetPidVid("33-23");
        CHECK(MaybePidVid);
        auto [pid, vid] = MaybePidVid.value();

        CHECK_EQ(pid, 33);
        CHECK_EQ(vid, 23);
    }
    SUBCASE("Valid doubledigit 4") {
        const auto MaybePidVid = GetPidVid("3-23");
        CHECK(MaybePidVid);
        auto [pid, vid] = MaybePidVid.value();

        CHECK_EQ(pid, 3);
        CHECK_EQ(vid, 23);
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
void TServer::RemoveClient(TClient& Client) {
    beammp_debug("removing client " + Client.Name.get() + " (" + std::to_string(ClientCount()) + ")");
    Client.ClearCars();
    WriteLock Lock(mClientsMutex);
    // erase all clients whose id matches, and those who have expired
    std::erase_if(mClients,
        [&](const std::weak_ptr<TClient>& C) {
            return C.expired()
                || C.lock()->ID.get() == Client.ID.get();
        });
}
void TServer::RemoveClient(const std::weak_ptr<TClient>& WeakClientPtr) {
    std::shared_ptr<TClient> LockedClientPtr { nullptr };
    try {
        LockedClientPtr = WeakClientPtr.lock();
    } catch (const std::exception&) {
        // silently fail, as there's nothing to do
        return;
    }
    beammp_assert(LockedClientPtr != nullptr);
    TClient& Client = *LockedClientPtr;
    RemoveClient(Client);
}

void TServer::ClaimFreeIDFor(TClient& ClientToChange) {
    ReadLock lock(mClientsMutex);
    int ID = 0;
    bool found = true;
    do {
        found = true;
        for (const auto& Client : mClients) {
            if (Client->ID.get() == ID) {
                found = false;
                ++ID;
            }
        }
    } while (!found);
    ClientToChange.ID = ID;
}

void TServer::ForEachClient(const std::function<bool(const std::shared_ptr<TClient>&)> Fn) {
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

void TServer::GlobalParser(const std::weak_ptr<TClient>& Client, std::vector<uint8_t>&& Packet, TPPSMonitor& PPSMonitor, TNetwork& Network) {
    constexpr std::string_view ABG = "ABG:";
    if (Packet.size() >= ABG.size() && std::equal(Packet.begin(), Packet.begin() + ABG.size(), ABG.begin(), ABG.end())) {
        Packet.erase(Packet.begin(), Packet.begin() + ABG.size());
        Packet = DeComp(Packet);
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

    std::string StringPacket(reinterpret_cast<const char*>(Packet.data()), Packet.size());

    // V to Y
    if (Code <= 89 && Code >= 86) {
        PPSMonitor.IncrementInternalPPS();
        Network.SendToAll(LockedClient.get(), Packet, false, false);
        return;
    }
    switch (Code) {
    case 'H': // initial connection
        if (!Network.SyncClient(Client)) {
            // TODO handle
        }
        return;
    case 'p':
        if (!Network.Respond(*LockedClient, StringToVector("p"), false)) {
            // failed to send
            Disconnect(LockedClient);
        } else {
            Network.UpdatePlayer(*LockedClient);
        }
        return;
    case 'O':
        if (Packet.size() > 1000) {
            beammp_debug(("Received data from: ") + LockedClient->Name.get() + (" Size: ") + std::to_string(Packet.size()));
        }
        ParseVehicle(*LockedClient, StringPacket, Network);
        return;
    case 'C': {
        if (Packet.size() < 4 || std::find(Packet.begin() + 3, Packet.end(), ':') == Packet.end())
            break;
        const auto PacketAsString = std::string(reinterpret_cast<const char*>(Packet.data()), Packet.size());
        std::string Message = "";
        const auto ColonPos = PacketAsString.find(':', 3);
        if (ColonPos != std::string::npos && ColonPos + 2 < PacketAsString.size()) {
            Message = PacketAsString.substr(ColonPos + 2);
        }
        if (Message.empty()) {
            beammp_debugf("Empty chat message received from '{}' ({}), ignoring it", LockedClient->Name.get(), LockedClient->ID.get());
            return;
        }
        auto Futures = LuaAPI::MP::Engine->TriggerEvent("onChatMessage", "", LockedClient->ID.get(), LockedClient->Name.get(), Message);
        TLuaEngine::WaitForAll(Futures);
        LogChatMessage(LockedClient->Name.get(), LockedClient->ID.get(), PacketAsString.substr(PacketAsString.find(':', 3) + 1));
        if (std::any_of(Futures.begin(), Futures.end(),
                [](const std::shared_ptr<TLuaResult>& Elem) {
                    return !Elem->Error
                        && Elem->Result.is<int>()
                        && bool(Elem->Result.as<int>());
                })) {
            break;
        }
        std::string SanitizedPacket = fmt::format("C:{}: {}", LockedClient->Name.get(), Message);
        Network.SendToAll(nullptr, StringToVector(SanitizedPacket), true, true);
        return;
    }
    case 'E':
        HandleEvent(*LockedClient, StringPacket);
        return;
    case 'N':
        beammp_trace("got 'N' packet (" + std::to_string(Packet.size()) + ")");
        Network.SendToAll(LockedClient.get(), Packet, false, true);
        return;
    case 'Z': // position packet
        PPSMonitor.IncrementInternalPPS();
        Network.SendToAll(LockedClient.get(), Packet, false, false);
        HandlePosition(*LockedClient, StringPacket);
        return;
    default:
        return;
    }
}

void TServer::HandleEvent(TClient& c, const std::string& RawData) {
    // E:Name:Data
    // Data is allowed to have ':'
    if (RawData.size() < 2) {
        beammp_debugf("Client '{}' ({}) tried to send an empty event, ignoring", c.Name.get(), c.ID.get());
        return;
    }
    auto NameDataSep = RawData.find(':', 2);
    if (NameDataSep == std::string::npos) {
        beammp_warn("received event in invalid format (missing ':'), got: '" + RawData + "'");
    }
    std::string Name = RawData.substr(2, NameDataSep - 2);
    std::string Data = RawData.substr(NameDataSep + 1);
    LuaAPI::MP::Engine->ReportErrors(LuaAPI::MP::Engine->TriggerEvent(Name, "", c.ID.get(), Data));
}

bool TServer::IsUnicycle(TClient& c, const std::string& CarJson) {
    try {
        auto Car = nlohmann::json::parse(CarJson);
        const std::string jbm = "jbm";
        if (Car.contains(jbm) && Car[jbm].is_string() && Car[jbm] == "unicycle") {
            return true;
        }
    } catch (const std::exception& e) {
        beammp_warn("Failed to parse vehicle data as json for client " + std::to_string(c.ID.get()) + ": '" + CarJson + "'.");
    }
    return false;
}

bool TServer::ShouldSpawn(TClient& c, const std::string& CarJson, int ID) {
    auto UnicycleID = c.UnicycleID.synchronize();
    if (IsUnicycle(c, CarJson) && *UnicycleID < 0) {
        *UnicycleID = ID;
        return true;
    } else {
        return c.GetCarCount() < Application::Settings.MaxCars;
    }
}

void TServer::ParseVehicle(TClient& c, const std::string& Pckt, TNetwork& Network) {
    if (Pckt.length() < 6)
        return;
    std::string Packet = Pckt;
    char Code = Packet.at(1);
    int PID = -1;
    int VID = -1;
    std::string Data = Packet.substr(3), pid, vid;
    switch (Code) { // Spawned Destroyed Switched/Moved NotFound Reset
    case 's':
        beammp_tracef("got 'Os' packet: '{}' ({})", Packet, Packet.size());
        if (Data.at(0) == '0') {
            int CarID = c.GetOpenCarID();
            beammp_debugf("'{}' created a car with ID {}", c.Name.get(), CarID);

            std::string CarJson = Packet.substr(5);
            Packet = "Os:" + c.Role.get() + ":" + c.Name.get() + ":" + std::to_string(c.ID.get()) + "-" + std::to_string(CarID) + ":" + CarJson;
            auto Futures = LuaAPI::MP::Engine->TriggerEvent("onVehicleSpawn", "", c.ID.get(), CarID, Packet.substr(3));
            TLuaEngine::WaitForAll(Futures);
            bool ShouldntSpawn = std::any_of(Futures.begin(), Futures.end(),
                [](const std::shared_ptr<TLuaResult>& Result) {
                    return !Result->Error && Result->Result.is<int>() && Result->Result.as<int>() != 0;
                });

            if (ShouldSpawn(c, CarJson, CarID) && !ShouldntSpawn) {
                c.AddNewCar(CarID, Packet);
                Network.SendToAll(nullptr, StringToVector(Packet), true, true);
            } else {
                if (!Network.Respond(c, StringToVector(Packet), true)) {
                    // TODO: handle
                }
                std::string Destroy = "Od:" + std::to_string(c.ID.get()) + "-" + std::to_string(CarID);
                if (!Network.Respond(c, StringToVector(Destroy), true)) {
                    // TODO: handle
                }
                beammp_debugf("{} (force : car limit/lua) removed ID {}", c.Name.get(), CarID);
            }
        }
        return;
    case 'c': {
        beammp_trace(std::string(("got 'Oc' packet: '")) + Packet + ("' (") + std::to_string(Packet.size()) + (")"));
        auto MaybePidVid = GetPidVid(Data.substr(0, Data.find(':', 1)));
        if (MaybePidVid) {
            std::tie(PID, VID) = MaybePidVid.value();
        }
        if (PID != -1 && VID != -1 && PID == c.ID.get()) {
            auto Futures = LuaAPI::MP::Engine->TriggerEvent("onVehicleEdited", "", c.ID.get(), VID, Packet.substr(3));
            TLuaEngine::WaitForAll(Futures);
            bool ShouldntAllow = std::any_of(Futures.begin(), Futures.end(),
                [](const std::shared_ptr<TLuaResult>& Result) {
                    return !Result->Error && Result->Result.is<int>() && Result->Result.as<int>() != 0;
                });

            auto FoundPos = Packet.find('{');
            FoundPos = FoundPos == std::string::npos ? 0 : FoundPos; // attempt at sanitizing this
            auto UnicycleID = c.UnicycleID.synchronize();
            if ((*UnicycleID != VID || IsUnicycle(c, Packet.substr(FoundPos)))
                && !ShouldntAllow) {
                Network.SendToAll(&c, StringToVector(Packet), false, true);
                Apply(c, VID, Packet);
            } else {
                if (*UnicycleID == VID) {
                    *UnicycleID = -1;
                }
                std::string Destroy = "Od:" + std::to_string(c.ID.get()) + "-" + std::to_string(VID);
                Network.SendToAll(nullptr, StringToVector(Destroy), true, true);
                c.DeleteCar(VID);
            }
        }
        return;
    }
    case 'd': {
        beammp_trace(std::string(("got 'Od' packet: '")) + Packet + ("' (") + std::to_string(Packet.size()) + (")"));
        auto MaybePidVid = GetPidVid(Data.substr(0, Data.find(':', 1)));
        if (MaybePidVid) {
            std::tie(PID, VID) = MaybePidVid.value();
        }
        if (PID != -1 && VID != -1 && PID == c.ID.get()) {
            auto UnicycleID = c.UnicycleID.synchronize();
            if (*UnicycleID == VID) {
                *UnicycleID = -1;
            }
            Network.SendToAll(nullptr, StringToVector(Packet), true, true);
            // TODO: should this trigger on all vehicle deletions?
            LuaAPI::MP::Engine->ReportErrors(LuaAPI::MP::Engine->TriggerEvent("onVehicleDeleted", "", c.ID.get(), VID));
            c.DeleteCar(VID);
            beammp_debug(c.Name.get() + (" deleted car with ID ") + std::to_string(VID));
        }
        return;
    }
    case 'r': {
        beammp_trace(std::string(("got 'Or' packet: '")) + Packet + ("' (") + std::to_string(Packet.size()) + (")"));
        auto MaybePidVid = GetPidVid(Data.substr(0, Data.find(':', 1)));
        if (MaybePidVid) {
            std::tie(PID, VID) = MaybePidVid.value();
        }

        if (PID != -1 && VID != -1 && PID == c.ID.get()) {
            Data = Data.substr(Data.find('{'));
            LuaAPI::MP::Engine->ReportErrors(LuaAPI::MP::Engine->TriggerEvent("onVehicleReset", "", c.ID.get(), VID, Data));
            Network.SendToAll(&c, StringToVector(Packet), false, true);
        }
        return;
    }
    case 't':
        beammp_trace(std::string(("got 'Ot' packet: '")) + Packet + ("' (") + std::to_string(Packet.size()) + (")"));
        Network.SendToAll(&c, StringToVector(Packet), false, true);
        return;
    case 'm':
        Network.SendToAll(&c, StringToVector(Packet), true, true);
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
        return;
    }
    std::string Header = VD.substr(0, VD.find('{'));

    FoundPos = VD.find('{');
    if (FoundPos == std::string::npos) {
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

struct PidVidData {
    int PID;
    int VID;
    std::string Data;
};

static std::optional<PidVidData> ParsePositionPacket(const std::string& Packet) {
    if (Packet.size() < 3) {
        // invalid packet
        return std::nullopt;
    }
    // Zp:PID-VID:DATA
    std::string withoutCode = Packet.substr(3);

    // parse veh ID
    if (auto DataBeginPos = withoutCode.find('{'); DataBeginPos != std::string::npos && DataBeginPos != 0) {
        // separator is :{, so position of { minus one
        auto PidVidOnly = withoutCode.substr(0, DataBeginPos - 1);
        auto MaybePidVid = GetPidVid(PidVidOnly);
        if (MaybePidVid) {
            int PID = -1;
            int VID = -1;
            // FIXME: check that the VID and PID are valid, so that we don't waste memory
            std::tie(PID, VID) = MaybePidVid.value();

            std::string Data = withoutCode.substr(DataBeginPos);
            return PidVidData {
                .PID = PID,
                .VID = VID,
                .Data = Data,
            };
        } else {
            // invalid packet
            return std::nullopt;
        }
    }
    // invalid packet
    return std::nullopt;
}

TEST_CASE("ParsePositionPacket") {
    const auto TestData = R"({"tim":10.428000331623,"vel":[-2.4171722121385e-05,-9.7184734153252e-06,-7.6420763232237e-06],"rot":[-0.0001296154171915,0.0031575385950029,0.98994906610295,0.14138903660382],"rvel":[5.3640324636461e-05,-9.9824529946024e-05,5.1664064641372e-05],"pos":[-0.27281248907838,-0.20515357944633,0.49695488960431],"ping":0.032999999821186})";
    SUBCASE("All the pids and vids") {
        for (int pid = 0; pid < 100; ++pid) {
            for (int vid = 0; vid < 100; ++vid) {
                std::optional<PidVidData> MaybeRes = ParsePositionPacket(fmt::format("Zp:{}-{}:{}", pid, vid, TestData));
                CHECK(MaybeRes.has_value());
                CHECK_EQ(MaybeRes.value().PID, pid);
                CHECK_EQ(MaybeRes.value().VID, vid);
                CHECK_EQ(MaybeRes.value().Data, TestData);
            }
        }
    }
}

void TServer::HandlePosition(TClient& c, const std::string& Packet) {
    if (auto Parsed = ParsePositionPacket(Packet); Parsed.has_value()) {
        c.SetCarPosition(Parsed.value().VID, Parsed.value().Data);
    }
}
