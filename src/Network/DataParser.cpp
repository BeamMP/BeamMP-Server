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
    char Code = Packet.at(0),SubCode = 0;
    if(Packet.length() > 1)SubCode = Packet.at(1);
    switch (Code) {
        case 'p':
            Respond("p",peer);
            return;
        case 'N':
            if(SubCode == 'R')peer->Name = (void *)Packet.substr(2).c_str();
            std::cout << "Name : " << (char *)peer->Name << std::endl;
            return;
    }
    std::cout << "Data : " << Packet << std::endl;
}
