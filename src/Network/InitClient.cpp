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
#include "Settings.h"
#include "UnixCompat.h"
#include <memory>

int OpenID() {
    int ID = 0;
    bool found;
    do {
        found = true;
        for (auto& c : CI->Clients) {
            if (c != nullptr) {
                if (c->GetID() == ID) {
                    found = false;
                    ID++;
                }
            }
        }
    } while (!found);
    return ID;
}
void Respond(Client* c, const std::string& MSG, bool Rel) {
    Assert(c);
    char C = MSG.at(0);
    if (Rel || C == 'W' || C == 'Y' || C == 'V' || C == 'E') {
        if (C == 'O' || C == 'T' || MSG.length() > 1000) {
            SendLarge(c, MSG);
        } else {
            TCPSend(c, MSG);
        }
    } else {
        UDPSend(c, MSG);
    }
}
void SendToAll(Client* c, const std::string& Data, bool Self, bool Rel) {
    if (!Self)
        Assert(c);
    char C = Data.at(0);
    for (auto& client : CI->Clients) {
        if (client != nullptr) {
            if (Self || client.get() != c) {
                if (client->isSynced) {
                    if (Rel || C == 'W' || C == 'Y' || C == 'V' || C == 'E') {
                        if (C == 'O' || C == 'T' || Data.length() > 1000)
                            SendLarge(client.get(), Data);
                        else
                            TCPSend(client.get(), Data);
                    } else
                        UDPSend(client.get(), Data);
                }
            }
        }
    }
}
void UpdatePlayers() {
    std::string Packet = ("Ss") + std::to_string(CI->Size()) + "/" + std::to_string(MaxPlayers) + ":";
    for (auto& c : CI->Clients) {
        if (c != nullptr)
            Packet += c->GetName() + ",";
    }
    Packet = Packet.substr(0, Packet.length() - 1);
    SendToAll(nullptr, Packet, true, true);
}
void OnDisconnect(Client* c, bool kicked) {
    Assert(c);
    info(c->GetName() + (" Connection Terminated"));
    std::string Packet;
    for (auto& v : c->GetAllCars()) {
        if (v != nullptr) {
            Packet = "Od:" + std::to_string(c->GetID()) + "-" + std::to_string(v->ID);
            SendToAll(c, Packet, false, true);
        }
    }
    if (kicked)
        Packet = ("L") + c->GetName() + (" was kicked!");
    Packet = ("L") + c->GetName() + (" Left the server!");
    SendToAll(c, Packet, false, true);
    Packet.clear();
    TriggerLuaEvent(("onPlayerDisconnect"), false, nullptr, std::make_unique<LuaArg>(LuaArg { { c->GetID() } }), false);
    if (c->GetTCPSock())
        CloseSocketProper(c->GetTCPSock());
    if (c->GetDownSock())
        CloseSocketProper(c->GetDownSock());
    CI->RemoveClient(c);
}
void OnConnect(Client* c) {
    Assert(c);
    info("Client connected");
    c->SetID(OpenID());
    info("Assigned ID " + std::to_string(c->GetID()) + " to " + c->GetName());
    TriggerLuaEvent("onPlayerConnecting", false, nullptr, std::make_unique<LuaArg>(LuaArg { { c->GetID() } }), false);
    SyncResources(c);
    if (c->GetStatus() < 0)
        return;
    Respond(c, "M" + MapName, true); //Send the Map on connect
    info(c->GetName() + " : Connected");
    TriggerLuaEvent("onPlayerJoining", false, nullptr, std::make_unique<LuaArg>(LuaArg { { c->GetID() } }), false);
}
