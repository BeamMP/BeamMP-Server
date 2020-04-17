///
/// Created by Anonymous275 on 2/4/2020.
///

#include <string>
#include "enet.hpp"
#include <fstream>
#include <iostream>
#include "../logger.h"
#include "../Settings.hpp"

void Respond(const std::string& MSG, ENetPeer*peer){
    enet_peer_send(peer, 0, enet_packet_create(MSG.c_str(), MSG.length() + 1, ENET_PACKET_FLAG_RELIABLE));
}

void SendToAll(ENetHost *server, ENetPeer*peer, const std::string& Data, bool All){
    for (int i = 0; i < server->connectedPeers; i++) {
        if (All || &server->peers[i] != peer) {
            enet_peer_send(&server->peers[i], 0, enet_packet_create(Data.c_str(),Data.length()+1, ENET_PACKET_FLAG_UNRELIABLE_FRAGMENT));
            enet_host_flush(server);
        }
    }
}

void OnConnect(ENetPeer*peer){
    enet_peer_send(peer, 0, enet_packet_create ("NR", 3, ENET_PACKET_FLAG_RELIABLE));
    //std::string ID = "P" + std::to_string(peer->serverVehicleID); /////HOLDUP
    //enet_peer_send(peer, 0, enet_packet_create (ID.c_str(), ID.length()+1, ENET_PACKET_FLAG_RELIABLE));
   //if(Debug)debug("ID : " + std::to_string(peer->serverVehicleID));
}
