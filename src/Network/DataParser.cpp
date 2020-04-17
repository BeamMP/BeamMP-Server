///
/// Created by Anonymous275 on 4/2/2020
///

#include <string>
#include "enet.hpp"
#include <vector>
#include <iostream>
#include "../logger.h"
#include "../Settings.hpp"

std::vector<std::string> Split(const std::string& String,const std::string& delimiter);
void SendToAll(ENetHost *server, ENetPeer*peer, const std::string& Data,bool All);
void Respond(const std::string& MSG, ENetPeer*peer);
int FindID(ENetHost *server,ENetPeer*peer);

void VehicleParser(std::string Packet,ENetPeer*peer,ENetHost*server){
    char Code = Packet.at(1);
    std::string Data = Packet.substr(3);
    std::vector<std::string> vector = Split(Packet,":");
    switch(Code){ //Spawned Destroyed Switched/Moved Reset
        case 's':
            if(!stoi(vector.at(0))){
                peer->serverVehicleID[0] = FindID(server,peer); ///TODO: WHAT IF IT IS THE SECOND VEHICLE?!
                vector.at(1) = std::to_string(peer->serverVehicleID[0]);
                Packet.clear();
                for(const std::string&a : vector)Packet += a + ":";
                Packet = Packet.substr(0,Packet.length()-1);
            }
            SendToAll(server,peer,Packet,true);
            break;
        case 'd':
            SendToAll(server,peer,Packet,true);
            break;
        case 'm':
            break;
        case 'r':
            SendToAll(server,peer,Packet,true);
            break;
    }
}

void ParseData(ENetPacket*packet, ENetPeer*peer, ENetHost*server){
    std::string Packet = (char*)packet->data;
    char Code = Packet.at(0),SubCode = 0;
    if(Packet.length() > 1)SubCode = Packet.at(1);
    switch (Code) {
        case 'p':
            Respond("p",peer);
            return;
        case 'N':
            if(SubCode == 'R')peer->Name = Packet.substr(2);
            std::cout << "Name : " << peer->Name << std::endl;
            return;
        case 'O':
            std::cout << peer->Name << " : " << Packet << std::endl;
            VehicleParser(Packet,peer,server);
            return;
    }
    //V to Z
    std::cout << peer->Name << " : " << Packet << std::endl;
    if(Code <= 90 && Code >= 86)SendToAll(server,peer,Packet,false);
    if(Debug)debug("Data : " + Packet);
}
