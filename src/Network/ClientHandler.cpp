//
// Created by Anonymous275 on 2/4/2020.
//
#include "enet.h"
#include <string>
#include <fstream>

using namespace std;
void NameRequest(ENetPeer*peer){
    ENetPacket* packet = enet_packet_create ("NameRequest", //Send A Name Request to the Client
                                             strlen ("NameRequest") + 1,
                                             ENET_PACKET_FLAG_RELIABLE); //Create A reliable packet using the data
    enet_peer_send(peer, 0, packet);
}

void SendToAll(ENetHost *server,ENetEvent event){
    ENetPacket* packet;
    for (int i = 0; i < server->connectedPeers; i++) {
        //if (&server->peers[i] != event.peer) { if you don't want to send it to the person ho just connected
            char Data[500];
            sprintf(Data,"There is %d Players Connected!",server->connectedPeers);
            printf("test %d\n",server->peers[i].serverVehicleID);

            packet = enet_packet_create(Data, strlen(Data)+1, 0);

            enet_peer_send(&server->peers[i], 0, packet);
            enet_host_flush(server);
        //}
    }
}

