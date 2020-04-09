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

void OnConnect(ENetPeer*peer,const std::string& data){
    std::vector<std::string> Data = Split(data,":");
    if(strcmp((char*)peer->Name,"Client information")==0){ //Checks if the Client has no name
        peer->Name = (void *)Data.at(0).c_str();
        char Info[100];
        info("Client Name is " + Data.at(0) + " ID : " + std::to_string(peer->connectID)); //ID System //Logs the data
        peer->serverVehicleID = (int)peer->connectID; //test to see if it works
        info(Data.at(0)+" ServerVehicleID : "+std::to_string(peer->serverVehicleID)+"  GameVehicleID : " + std::to_string(peer->gameVehicleID[0]));
    }
}
