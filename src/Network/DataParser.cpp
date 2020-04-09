///
/// Created by Anonymous275 on 4/2/2020
///

#include "enet.h"
#include <iostream>
#include <string>
#include <vector>
#include "../logger.h"

std::vector<std::string> Split(const std::string& String,const std::string& delimiter);

void ParseData(ENetPacket*packet,ENetPeer*peer){ //here we will parse the data
    std::string Packet = (char*)packet->data;
    int off = stoi(Packet.substr(0, 2));
    std::string header = Packet.substr(2, off - 2), data = Packet.substr(off);
    std::vector<std::string> split;

    if(!header.empty()) {
        std::cout << header << " header size : " << header.size() << std::endl;
        split = Split(header, ":"); //1st is reliable - 2nd is Code - 3rd is VehID
    }
    if(!data.empty()){
        std::cout << data << std::endl;
    }

}