// Copyright (c) 2019-present Anonymous275.
// BeamMP Server code is not in the public domain and is not free software.
// One must be granted explicit permission by the copyright holder in order to modify or distribute any part of the source or binaries.
// Anything else is prohibited. Modified works may not be published and have be upstreamed to the official repository.
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
#undef GetObject //to fix microsoft bs
#include "Json.h"

void Apply(Client* c, int VID, const std::string& pckt) {
    Assert(c);
    std::string Packet = pckt.substr(pckt.find('{')), VD = c->GetCarData(VID);
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
    c->SetCarData(VID, Header + Buffer.GetString());
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
        debug(std::string(("got 'Os' packet: '")) + Packet + ("' (") + std::to_string(Packet.size()) + (")"));
#endif
        if (Data.at(0) == '0') {
            int CarID = c->GetOpenCarID();
            debug(c->GetName() + (" created a car with ID ") + std::to_string(CarID));
            Packet = "Os:" + c->GetRoles() + ":" + c->GetName() + ":" + std::to_string(c->GetID()) + "-" + std::to_string(CarID) + Packet.substr(4);
            auto Res = TriggerLuaEvent(("onVehicleSpawn"), false, nullptr, std::make_unique<LuaArg>(LuaArg { { c->GetID(), CarID, Packet.substr(3) } }), true);
            if (c->GetCarCount() >= MaxCars || std::any_cast<int>(Res)) {
                Respond(c, Packet, true);
                std::string Destroy = "Od:" + std::to_string(c->GetID()) + "-" + std::to_string(CarID);
                Respond(c, Destroy, true);
                debug(c->GetName() + (" (force : car limit/lua) removed ID ") + std::to_string(CarID));
            } else {
                c->AddNewCar(CarID, Packet);
                SendToAll(nullptr, Packet, true, true);
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
        if (PID != -1 && VID != -1 && PID == c->GetID()) {
            auto Res = TriggerLuaEvent(("onVehicleEdited"), false, nullptr,
                std::make_unique<LuaArg>(LuaArg { { c->GetID(), VID, Packet.substr(3) } }),
                true);
            if (!std::any_cast<int>(Res)) {
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
        debug(std::string(("got 'Od' packet: '")) + Packet + ("' (") + std::to_string(Packet.size()) + (")"));
#endif
        pid = Data.substr(0, Data.find('-'));
        vid = Data.substr(Data.find('-') + 1);
        if (pid.find_first_not_of("0123456789") == std::string::npos && vid.find_first_not_of("0123456789") == std::string::npos) {
            PID = stoi(pid);
            VID = stoi(vid);
        }
        if (PID != -1 && VID != -1 && PID == c->GetID()) {
            SendToAll(nullptr, Packet, true, true);
            TriggerLuaEvent(("onVehicleDeleted"), false, nullptr,
                std::make_unique<LuaArg>(LuaArg { { c->GetID(), VID } }), false);
            c->DeleteCar(VID);
            debug(c->GetName() + (" deleted car with ID ") + std::to_string(VID));
        }
        return;
    case 'r':
#ifdef DEBUG
        debug(std::string(("got 'Or' packet: '")) + Packet + ("' (") + std::to_string(Packet.size()) + (")"));
#endif
        SendToAll(c, Packet, false, true);
        return;
    case 't':
#ifdef DEBUG
        debug(std::string(("got 'Ot' packet: '")) + Packet + ("' (") + std::to_string(Packet.size()) + (")"));
#endif
        SendToAll(c, Packet, false, true);
        return;
    default:
#ifdef DEBUG
        warn(std::string(("possibly not implemented: '") + Packet + ("' (") + std::to_string(Packet.size()) + (")")));
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
    Respond(c, ("Sn") + c->GetName(), true);
    SendToAll(c, ("JWelcome ") + c->GetName() + "!", false, true);
    TriggerLuaEvent(("onPlayerJoin"), false, nullptr, std::make_unique<LuaArg>(LuaArg { { c->GetID() } }), false);
    for (auto& client : CI->Clients) {
        if (client != nullptr) {
            if (client.get() != c) {
                for (auto& v : client->GetAllCars()) {
                    if (v != nullptr) {
                        if (c->GetStatus() < 0)
                            return;
                        Respond(c, v->Data, true);
                        std::this_thread::sleep_for(std::chrono::seconds(2));
                    }
                }
            }
        }
    }
    info(c->GetName() + (" is now synced!"));
}
void ParseVeh(Client* c, const std::string& Packet) {
    Assert(c);
#ifdef WIN32
    __try {
        VehicleParser(c, Packet);
    } __except (Handle(GetExceptionInformation(), ("Vehicle Handler"))) { }
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
            TriggerLuaEvent(Name, false, nullptr, std::make_unique<LuaArg>(LuaArg { { c->GetID(), t } }), false);
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
    std::any Res;
    std::string Packet = Pack.substr(0, strlen(Pack.c_str()));
    char Code = Packet.at(0);

    //V to Z
    if (Code <= 90 && Code >= 86) {
        PPS++;
        SendToAll(c, Packet, false, false);
        return;
    }
    switch (Code) {
    case 'H': // initial connection
#ifdef DEBUG
        debug(std::string("got 'H' packet: '") + Pack + "' (" + std::to_string(Packet.size()) + ")");
#endif
        SyncClient(c);
        return;
    case 'p':
        Respond(c, ("p"), false);
        UpdatePlayers();
        return;
    case 'O':
        if (Packet.length() > 1000) {
            debug(("Received data from: ") + c->GetName() + (" Size: ") + std::to_string(Packet.length()));
        }
        ParseVeh(c, Packet);
        return;
    case 'J':
#ifdef DEBUG
        debug(std::string(("got 'J' packet: '")) + Pack + ("' (") + std::to_string(Packet.size()) + (")"));
#endif
        SendToAll(c, Packet, false, true);
        return;
    case 'C':
#ifdef DEBUG
        debug(std::string(("got 'C' packet: '")) + Pack + ("' (") + std::to_string(Packet.size()) + (")"));
#endif
        if (Packet.length() < 4 || Packet.find(':', 3) == std::string::npos)
            break;
        Res = TriggerLuaEvent("onChatMessage", false, nullptr, std::make_unique<LuaArg>(LuaArg { { c->GetID(), c->GetName(), Packet.substr(Packet.find(':', 3) + 1) } }), true);
        if (std::any_cast<int>(Res))
            break;
        SendToAll(nullptr, Packet, true, true);
        return;
    case 'E':
#ifdef DEBUG
        debug(std::string(("got 'E' packet: '")) + Pack + ("' (") + std::to_string(Packet.size()) + (")"));
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
    } __except (Handle(GetExceptionInformation(), ("Global Handler"))) { }
#else
    GlobalParser(c, Packet);
#endif // WIN32
}
