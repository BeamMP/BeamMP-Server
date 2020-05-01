///
/// Created by Anonymous275 on 2/4/2020.
///

#include <string>
#include "enet.hpp"
#include <fstream>
#include <iostream>
#include "../logger.h"
#include "../Settings.hpp"

int FindID(ENetHost *server,ENetPeer*peer);
void Respond(const std::string& MSG, ENetPeer*peer){
    enet_peer_send(peer, 0, enet_packet_create(MSG.c_str(), MSG.length() + 1, ENET_PACKET_FLAG_RELIABLE));
}

void SendToAll(ENetHost *server, ENetPeer*peer,const std::string& Data, bool All, bool Reliable){
    //std::cout << "Sending Code " << Data.at(0) << " length:" << Data.length() << " to all with the self switch : " << All << std::endl;
    for (int i = 0; i < server->connectedPeers; i++) {
        if (All || &server->peers[i] != peer) {
            //reliable is 1 unreliable is 8
            enet_peer_send(&server->peers[i], 0, enet_packet_create(Data.c_str(),Data.length()+1,Reliable?1:8));
            enet_host_flush(server);
        }
    }
}
void UpdatePlayers(ENetHost *server,ENetPeer*peer){
    std::string Packet = "Ss" + std::to_string(server->connectedPeers)+"/"+std::to_string(MaxPlayers) + ":";
    for (int i = 0; i < server->connectedPeers; i++) {
        ENetPeer*SPeer = &server->peers[i];
        Packet += SPeer->Name + ",";
    }
    Packet = Packet.substr(0,Packet.length()-1);
    SendToAll(server,peer,Packet,true,true);
}

void OnDisconnect(ENetHost *server,ENetPeer*peer,bool Timed){
    std::string Packet = "Od:" + std::to_string(peer->serverVehicleID[0]);
    SendToAll(server,peer, Packet,false,true);
    if(Timed)Packet = "L"+peer->Name+" Timed out!";
    else Packet = "L"+peer->Name+" Left the server!";
    SendToAll(server,peer, Packet,false,true);
    UpdatePlayers(server,peer);
    Packet.clear();
    peer->DID.clear();
    peer->Name.clear();
    peer->VehicleData.clear();
}

void OnConnect(ENetHost *server,ENetPeer*peer){
    Respond("NR",peer);
    peer->serverVehicleID[0] = FindID(server,peer);  ///TODO: WHAT IF IT IS THE SECOND VEHICLE?
    std::string ID = "P" + std::to_string(peer->serverVehicleID[0]);
    Respond(ID,peer);
    if(Debug)debug(peer->Name + " ID : " + std::to_string(peer->serverVehicleID[0]));
}
