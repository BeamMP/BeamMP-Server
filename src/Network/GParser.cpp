///
/// Created by Anonymous275 on 8/1/2020
///
#include "Client.hpp"
#include "Logger.h"
#include "Lua/LuaSystem.hpp"
#include "Network.h"
#include "Security/Enc.h"
#include "Settings.h"
#include "UnixCompat.h"
#include <memory>
#include <sstream>

int FC(const std::string& s, const std::string& p, int n) {
    auto i = s.find(p);
    int j;
    for (j = 1; j < n && i != std::string::npos; ++j) {
        i = s.find(p, i + 1);
    }
    if (j == n)
        return int(i);
    else
        return -1;
}
void Apply(Client* c, int VID, const std::string& pckt) {
    Assert(c);
    std::string Packet = pckt;
    std::string VD = c->GetCarData(VID);
    Packet = Packet.substr(FC(Packet, ",", 2) + 1);
    Packet = VD.substr(0, FC(VD, ",", 2) + 1) + Packet.substr(0, Packet.find_last_of('"') + 1) + VD.substr(FC(VD, ",\"", 7));
    c->SetCarData(VID, Packet);
}

void VehicleParser(Client* c, const std::string& Pckt) {
    Assert(c);
    if (c == nullptr || Pckt.length() < 4)
        return;
    std::string Packet = Pckt;
    char Code = Packet.at(1);
    int PID = -1;
    int VID = -1;
    std::string Data = Packet.substr(3), pid, vid;
    switch (Code) { //Spawned Destroyed Switched/Moved NotFound Reset
    case 's':
#ifdef DEBUG
        debug(std::string(Sec("got 'Os' packet: '")) + Packet + Sec("' (") + std::to_string(Packet.size()) + Sec(")"));
#endif
        if (Data.at(0) == '0') {
            int CarID = c->GetOpenCarID();
            debug(c->GetName() + Sec(" created a car with ID ") + std::to_string(CarID));
            Packet = "Os:" + c->GetRole() + ":" + c->GetName() + ":" + std::to_string(c->GetID()) + "-" + std::to_string(CarID) + Packet.substr(4);
            if (c->GetCarCount() >= MaxCars || TriggerLuaEvent(Sec("onVehicleSpawn"), false, nullptr, std::make_unique<LuaArg>(LuaArg { { c->GetID(), CarID, Packet.substr(3) } }), true)) {
                Respond(c, Packet, true);
                std::string Destroy = "Od:" + std::to_string(c->GetID()) + "-" + std::to_string(CarID);
                Respond(c, Destroy, true);
                debug(c->GetName() + Sec(" (force : car limit/lua) removed ID ") + std::to_string(CarID));
            } else {
                c->AddNewCar(CarID, Packet);
                SendToAll(nullptr, Packet, true, true);
            }
        }
        return;
    case 'c':
#ifdef DEBUG
        debug(std::string(Sec("got 'Oc' packet: '")) + Packet + Sec("' (") + std::to_string(Packet.size()) + Sec(")"));
#endif
        pid = Data.substr(0, Data.find('-'));
        vid = Data.substr(Data.find('-') + 1, Data.find(':', 1) - Data.find('-') - 1);
        if (pid.find_first_not_of("0123456789") == std::string::npos && vid.find_first_not_of("0123456789") == std::string::npos) {
            PID = stoi(pid);
            VID = stoi(vid);
        }
        if (PID != -1 && VID != -1 && PID == c->GetID()) {
            if (!TriggerLuaEvent(Sec("onVehicleEdited"), false, nullptr,
                    std::unique_ptr<LuaArg>(new LuaArg { { c->GetID(), VID, Packet.substr(3) } }),
                    true)) {
                SendToAll(c, Packet, false, true);
                Apply(c, VID, Packet);
            } else {
                std::string Destroy = "Od:" + std::to_string(c->GetID()) + "-" + std::to_string(VID);
                Respond(c, Destroy, true);
                c->DeleteCar(VID);
            }
        }
        return;
    case 'd':
#ifdef DEBUG
        debug(std::string(Sec("got 'Od' packet: '")) + Packet + Sec("' (") + std::to_string(Packet.size()) + Sec(")"));
#endif
        pid = Data.substr(0, Data.find('-'));
        vid = Data.substr(Data.find('-') + 1);
        if (pid.find_first_not_of("0123456789") == std::string::npos && vid.find_first_not_of("0123456789") == std::string::npos) {
            PID = stoi(pid);
            VID = stoi(vid);
        }
        if (PID != -1 && VID != -1 && PID == c->GetID()) {
            SendToAll(nullptr, Packet, true, true);
            TriggerLuaEvent(Sec("onVehicleDeleted"), false, nullptr,
                std::unique_ptr<LuaArg>(new LuaArg { { c->GetID(), VID } }), false);
            c->DeleteCar(VID);
            debug(c->GetName() + Sec(" deleted car with ID ") + std::to_string(VID));
        }
        return;
    case 'r':
#ifdef DEBUG
        debug(std::string(Sec("got 'Or' packet: '")) + Packet + Sec("' (") + std::to_string(Packet.size()) + Sec(")"));
#endif
        SendToAll(c, Packet, false, true);
        return;
    case 't':
#ifdef DEBUG
        debug(std::string(Sec("got 'Ot' packet: '")) + Packet + Sec("' (") + std::to_string(Packet.size()) + Sec(")"));
#endif
        SendToAll(c, Packet, false, true);
        return;
    default:
#ifdef DEBUG
        warn(std::string(Sec("possibly not implemented: '") + Packet + Sec("' (") + std::to_string(Packet.size()) + Sec(")")));
#endif // DEBUG
        return;
    }
}
void SyncClient(Client* c) {
    Assert(c);
    if (c->isSynced)
        return;
    c->isSynced = true;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    Respond(c, Sec("Sn") + c->GetName(), true);
    SendToAll(c, Sec("JWelcome ") + c->GetName() + "!", false, true);
    TriggerLuaEvent(Sec("onPlayerJoin"), false, nullptr, std::unique_ptr<LuaArg>(new LuaArg { { c->GetID() } }), false);
    for (auto& client : CI->Clients) {
        if (client != nullptr) {
            if (client.get() != c) {
                for (auto& v : client->GetAllCars()) {
                    if (v != nullptr) {
                        if(c->GetStatus() < 0)return;
                        Respond(c, v->Data, true);
                        std::this_thread::sleep_for(std::chrono::seconds(2));
                    }
                }
            }
        }
    }
    info(c->GetName() + Sec(" is now synced!"));
}
void ParseVeh(Client* c, const std::string& Packet) {
    Assert(c);
#ifdef WIN32
    __try {
        VehicleParser(c, Packet);
    } __except (Handle(GetExceptionInformation(), Sec("Vehicle Handler"))) { }
#else // unix
    VehicleParser(c, Packet);
#endif // WIN32
}

void HandleEvent(Client* c, const std::string& Data) {
    Assert(c);
    std::stringstream ss(Data);
    std::string t, Name;
    int a = 0;
    while (std::getline(ss, t, ':')) {
        switch (a) {
        case 1:
            Name = t;
            break;
        case 2:
            TriggerLuaEvent(Name, false, nullptr, std::unique_ptr<LuaArg>(new LuaArg { { c->GetID(), t } }), false);
            break;
        default:
            break;
        }
        if (a == 2)
            break;
        a++;
    }
}

void GlobalParser(Client* c, const std::string& Pack) {
    Assert(c);
    if (Pack.empty() || c == nullptr)
        return;
    std::string Packet = Pack.substr(0, strlen(Pack.c_str()));
    std::string pct;
    char Code = Packet.at(0);

    //V to Z
    if (Code <= 90 && Code >= 86) {
        PPS++;
        SendToAll(c, Packet, false, false);
        return;
    }
    switch (Code) {
    case 'P': // initial connection
#ifdef DEBUG
        debug(std::string(Sec("got 'P' packet: '")) + Pack + Sec("' (") + std::to_string(Packet.size()) + Sec(")"));
#endif
        Respond(c, Sec("P") + std::to_string(c->GetID()), true);
        SyncClient(c);
        return;
    case 'p':
        Respond(c, Sec("p"), false);
        UpdatePlayers();
        return;
    case 'O':
        if (Packet.length() > 1000) {
            debug(Sec("Received data from: ") + c->GetName() + Sec(" Size: ") + std::to_string(Packet.length()));
        }
        ParseVeh(c, Packet);
        return;
    case 'J':
#ifdef DEBUG
        debug(std::string(Sec("got 'J' packet: '")) + Pack + Sec("' (") + std::to_string(Packet.size()) + Sec(")"));
#endif
        SendToAll(c, Packet, false, true);
        return;
    case 'C':
#ifdef DEBUG
        debug(std::string(Sec("got 'C' packet: '")) + Pack + Sec("' (") + std::to_string(Packet.size()) + Sec(")"));
#endif
        if (Packet.length() < 4 || Packet.find(':', 3) == std::string::npos)
            break;
        if (TriggerLuaEvent(Sec("onChatMessage"), false, nullptr,
                std::unique_ptr<LuaArg>(new LuaArg {
                    { c->GetID(), c->GetName(), Packet.substr(Packet.find(':', 3) + 1) } }),
                true))
            break;
        SendToAll(nullptr, Packet, true, true);
        return;
    case 'E':
#ifdef DEBUG
        debug(std::string(Sec("got 'E' packet: '")) + Pack + Sec("' (") + std::to_string(Packet.size()) + Sec(")"));
#endif
        HandleEvent(c, Packet);
        return;
    default:
        return;
    }
}

void GParser(Client* c, const std::string& Packet) {
    Assert(c);
    if (Packet.find("Zp") != std::string::npos && Packet.size() > 500) {
        abort();
    }
#ifdef WIN32
    __try {
        GlobalParser(c, Packet);
    } __except (Handle(GetExceptionInformation(), Sec("Global Handler"))) { }
#else
    GlobalParser(c, Packet);
#endif // WIN32
}
