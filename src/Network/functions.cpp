///
/// Created by Anonymous275 on 4/1/2020
///
#include "enet.h"
#include <vector>
#include "../logger.h"

std::vector<std::string> Split(const std::string& String,const std::string& delimiter){
    std::vector<std::string> Val;
    size_t pos = 0;
    std::string token,s = String;
    while ((pos = s.find(delimiter)) != std::string::npos) {
        token = s.substr(0, pos);
        Val.push_back(token);
        s.erase(0, pos + delimiter.length());
    }
    Val.push_back(s);
    return Val;
}

void OnConnect(ENetPeer*peer){
    ENetPacket* packet = enet_packet_create ("NameRequest", //Send A Name Request to the Client
                                             strlen ("NameRequest") + 1,
                                             ENET_PACKET_FLAG_RELIABLE); //Create A reliable packet using the data
    enet_peer_send(peer, 0, packet);
    std::cout << "ID : " << peer->serverVehicleID << std::endl;
}
