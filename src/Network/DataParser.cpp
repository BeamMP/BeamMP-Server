///
/// Created by Anonymous275 on 4/2/2020
///

#include "enet.h"
#include <iostream>
#include <string>
#include <vector>
#include "../logger.h"

void Respond(const std::string& MSG, ENetPeer*peer);
void ParseData(ENetPacket*packet, ENetPeer*peer){
    std::string Packet = (char*)packet->data;
    if(Packet == "p"){Respond("p",peer);return;}
    std::cout << "Data : " << Packet << std::endl;
}
