#include "TServer.h"
#include "Client.h"
#include "Common.h"
#include "TPPSMonitor.h"
#include "TNetwork.h"
#include <TLuaFile.h>
#include <any>
#include <sstream>

#undef GetObject //Fixes Windows

#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

namespace json = rapidjson;

TServer::TServer(int argc, char** argv) {
    info("BeamMP Server running version " + Application::ServerVersion());
    if (argc > 1) {
        Application::Settings.CustomIP = argv[1];
        size_t n = std::count(Application::Settings.CustomIP.begin(), Application::Settings.CustomIP.end(), '.');
        auto p = Application::Settings.CustomIP.find_first_not_of(".0123456789");
        if (p != std::string::npos || n != 3 || Application::Settings.CustomIP.substr(0, 3) == "127") {
            Application::Settings.CustomIP.clear();
            warn("IP Specified is invalid! Ignoring");
        } else {
            info("server started with custom IP");
        }
    }
}

void TServer::RemoveClient(const std::weak_ptr<TClient>& WeakClientPtr) {
    if (!WeakClientPtr.expired()) {
        TClient& Client = *WeakClientPtr.lock();
        debug("removing client " + Client.GetName() + " (" + std::to_string(ClientCount()) + ")");
        Client.ClearCars();
        WriteLock Lock(mClientsMutex);
        mClients.erase(WeakClientPtr.lock());
    }
}

std::weak_ptr<TClient> TServer::InsertNewClient() {
    debug("inserting new client (" + std::to_string(ClientCount()) + ")");
    WriteLock Lock(mClientsMutex);
    auto [Iter, Replaced] = mClients.insert(std::make_shared<TClient>(*this));
    return *Iter;
}

void TServer::ForEachClient(const std::function<bool(std::weak_ptr<TClient>)>& Fn) {
    ReadLock Lock(mClientsMutex);
    for (auto& Client : mClients) {
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
        abort();
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

    //V to Z
    if (Code <= 90 && Code >= 86) {
        PPSMonitor.IncrementInternalPPS();
        Network.SendToAll(LockedClient.get(), Packet, false, false);
        return;
    }
    switch (Code) {
    case 'H': // initial connection
#ifdef DEBUG
        debug(std::string("got 'H' packet: '") + Packet + "' (" + std::to_string(Packet.size()) + ")");
#endif
        Network.SyncClient(Client);
        return;
    case 'p':
        Network.Respond(*LockedClient, ("p"), false);
        Network.UpdatePlayer(*LockedClient);
        LockedClient->UpdatePingTime();
        return;
    case 'O':
        if (Packet.length() > 1000) {
            debug(("Received data from: ") + LockedClient->GetName() + (" Size: ") + std::to_string(Packet.length()));
        }
        ParseVehicle(*LockedClient, Packet, Network);
        return;
    case 'J':
#ifdef DEBUG
        debug(std::string(("got 'J' packet: '")) + Packet + ("' (") + std::to_string(Packet.size()) + (")"));
#endif
        Network.SendToAll(LockedClient.get(), Packet, false, true);
        return;
    case 'C':
#ifdef DEBUG
        debug(std::string(("got 'C' packet: '")) + Packet + ("' (") + std::to_string(Packet.size()) + (")"));
#endif
        if (Packet.length() < 4 || Packet.find(':', 3) == std::string::npos)
            break;
        Res = TriggerLuaEvent("onChatMessage", false, nullptr, std::make_unique<TLuaArg>(TLuaArg { { LockedClient->GetID(), LockedClient->GetName(), Packet.substr(Packet.find(':', 3) + 1) } }), true);
        if (std::any_cast<int>(Res))
            break;
        Network.SendToAll(nullptr, Packet, true, true);
        return;
    case 'E':
#ifdef DEBUG
        debug(std::string(("got 'E' packet: '")) + Packet + ("' (") + std::to_string(Packet.size()) + (")"));
#endif
        HandleEvent(*LockedClient, Packet);
        return;
    default:
        return;
    }
}

void TServer::HandleEvent(TClient& c, const std::string& Data) {
    std::stringstream ss(Data);
    std::string t, Name;
    int a = 0;
    while (std::getline(ss, t, ':')) {
        switch (a) {
        case 1:
            Name = t;
            break;
        case 2:
            TriggerLuaEvent(Name, false, nullptr, std::make_unique<TLuaArg>(TLuaArg { { c.GetID(), t } }), false);
            break;
        default:
            break;
        }
        if (a == 2)
            break;
        a++;
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
    switch (Code) { //Spawned Destroyed Switched/Moved NotFound Reset
    case 's':
#ifdef DEBUG
        debug(std::string(("got 'Os' packet: '")) + Packet + ("' (") + std::to_string(Packet.size()) + (")"));
#endif
        if (Data.at(0) == '0') {
            int CarID = c.GetOpenCarID();
            debug(c.GetName() + (" created a car with ID ") + std::to_string(CarID));
            Packet = "Os:" + c.GetRoles() + ":" + c.GetName() + ":" + std::to_string(c.GetID()) + "-" + std::to_string(CarID) + Packet.substr(4);
            auto Res = TriggerLuaEvent(("onVehicleSpawn"), false, nullptr, std::make_unique<TLuaArg>(TLuaArg { { c.GetID(), CarID, Packet.substr(3) } }), true);
            if (c.GetCarCount() >= Application::Settings.MaxCars || std::any_cast<int>(Res)) {
                Network.Respond(c, Packet, true);
                std::string Destroy = "Od:" + std::to_string(c.GetID()) + "-" + std::to_string(CarID);
                Network.Respond(c, Destroy, true);
                debug(c.GetName() + (" (force : car limit/lua) removed ID ") + std::to_string(CarID));
            } else {
                c.AddNewCar(CarID, Packet);
                Network.SendToAll(nullptr, Packet, true, true);
            }
        }
        return;
    case 'c':
#ifdef DEBUG
        debug(std::string(("got 'Oc' packet: '")) + Packet + ("' (") + std::to_string(Packet.size()) + (")"));
#endif
        pid = Data.substr(0, Data.find('-'));
        vid = Data.substr(Data.find('-') + 1, Data.find(':', 1) - Data.find('-') - 1);
        if (pid.find_first_not_of("0123456789") == std::string::npos && vid.find_first_not_of("0123456789") == std::string::npos) {
            PID = stoi(pid);
            VID = stoi(vid);
        }
        if (PID != -1 && VID != -1 && PID == c.GetID()) {
            auto Res = TriggerLuaEvent(("onVehicleEdited"), false, nullptr,
                std::make_unique<TLuaArg>(TLuaArg { { c.GetID(), VID, Packet.substr(3) } }),
                true);
            if (!std::any_cast<int>(Res)) {
                Network.SendToAll(&c, Packet, false, true);
                Apply(c, VID, Packet);
            } else {
                std::string Destroy = "Od:" + std::to_string(c.GetID()) + "-" + std::to_string(VID);
                Network.Respond(c, Destroy, true);
                c.DeleteCar(VID);
            }
        }
        return;
    case 'd':
#ifdef DEBUG
        debug(std::string(("got 'Od' packet: '")) + Packet + ("' (") + std::to_string(Packet.size()) + (")"));
#endif
        pid = Data.substr(0, Data.find('-'));
        vid = Data.substr(Data.find('-') + 1);
        if (pid.find_first_not_of("0123456789") == std::string::npos && vid.find_first_not_of("0123456789") == std::string::npos) {
            PID = stoi(pid);
            VID = stoi(vid);
        }
        if (PID != -1 && VID != -1 && PID == c.GetID()) {
            Network.SendToAll(nullptr, Packet, true, true);
            TriggerLuaEvent(("onVehicleDeleted"), false, nullptr,
                std::make_unique<TLuaArg>(TLuaArg { { c.GetID(), VID } }), false);
            c.DeleteCar(VID);
            debug(c.GetName() + (" deleted car with ID ") + std::to_string(VID));
        }
        return;
    case 'r':
#ifdef DEBUG
        debug(std::string(("got 'Or' packet: '")) + Packet + ("' (") + std::to_string(Packet.size()) + (")"));
#endif
        Pos = int(Data.find('-'));
        pid = Data.substr(0, Pos++);
        vid = Data.substr(Pos, Data.find(':') - Pos);

        if (pid.find_first_not_of("0123456789") == std::string::npos && vid.find_first_not_of("0123456789") == std::string::npos) {
            PID = stoi(pid);
            VID = stoi(vid);
        }

        if (PID != -1 && VID != -1 && PID == c.GetID()) {
            Data = Data.substr(Data.find('{'));
            TriggerLuaEvent("onVehicleReset", false, nullptr,
                std::make_unique<TLuaArg>(TLuaArg { { c.GetID(), VID, Data } }),
                false);
            Network.SendToAll(&c, Packet, false, true);
        }
        return;
    case 't':
#ifdef DEBUG
        debug(std::string(("got 'Ot' packet: '")) + Packet + ("' (") + std::to_string(Packet.size()) + (")"));
#endif
        Network.SendToAll(&c, Packet, false, true);
        return;
    default:
#ifdef DEBUG
        warn(std::string(("possibly not implemented: '") + Packet + ("' (") + std::to_string(Packet.size()) + (")")));
#endif // DEBUG
        return;
    }
}

void TServer::Apply(TClient& c, int VID, const std::string& pckt) {
    std::string Packet = pckt.substr(pckt.find('{')), VD = c.GetCarData(VID);
    std::string Header = VD.substr(0, VD.find('{'));
    VD = VD.substr(VD.find('{'));
    rapidjson::Document Veh, Pack;
    Veh.Parse(VD.c_str());
    if (Veh.HasParseError()) {
        error("Could not get vehicle config!");
        return;
    }
    Pack.Parse(Packet.c_str());
    if (Pack.HasParseError() || Pack.IsNull()) {
        error("Could not get active vehicle config!");
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
    debug("inserting client (" + std::to_string(ClientCount()) + ")");
    WriteLock Lock(mClientsMutex);
    (void)mClients.insert(NewClient);
}
