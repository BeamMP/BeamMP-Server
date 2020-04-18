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
    std::cout << "Sending Code " << Data.at(0) << " length:" << Data.length() << " to all with the self switch : " << All << std::endl;
    for (int i = 0; i < server->connectedPeers; i++) {
        if (All || &server->peers[i] != peer) {
            //reliable is 1 unreliable is 8
            enet_peer_send(&server->peers[i], 0, enet_packet_create(Data.c_str(),Data.length()+1,Reliable?1:8));
            enet_host_flush(server);
        }
    }
}

void OnConnect(ENetHost *server,ENetPeer*peer){
    enet_peer_send(peer, 0, enet_packet_create ("NR", 3, ENET_PACKET_FLAG_RELIABLE));
    peer->serverVehicleID[0] = FindID(server,peer);  ///TODO: WHAT IF IT IS THE SECOND VEHICLE?
    std::string ID = "P" + std::to_string(peer->serverVehicleID[0]);
    enet_peer_send(peer, 0, enet_packet_create (ID.c_str(), ID.length()+1, ENET_PACKET_FLAG_RELIABLE));
    if(Debug)debug(peer->Name + " ID : " + std::to_string(peer->serverVehicleID[0]));
}
