// Copyright (c) 2020 Anonymous275.
// BeamMP Server code is not in the public domain and is not free software.
// One must be granted explicit permission by the copyright holder in order to modify or distribute any part of the source or binaries.
// Anything else is prohibited. Modified works may not be published and have be upstreamed to the official repository.
///
/// Created by Anonymous275 on 7/31/2020
///
#pragma once
#include "Client.hpp"
#include <string>
void TCPServerMain();
void UpdatePlayers();
void OnConnect(Client* c);
void TCPClient(Client* c);
std::string TCPRcv(Client* c);
void SyncResources(Client* c);
[[noreturn]] void UDPServerMain();
void OnDisconnect(Client* c, bool kicked);
void UDPSend(Client* c, std::string Data);
void SendLarge(Client* c, std::string Data);
bool TCPSend(Client* c, const std::string& Data);
void GParser(Client* c, const std::string& Packet);
std::string StaticReason(bool Set,const std::string& R);
void Respond(Client* c, const std::string& MSG, bool Rel);
void SendToAll(Client* c, const std::string& Data, bool Self, bool Rel);
