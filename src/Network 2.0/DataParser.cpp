///
/// Created by Anonymous275 on 4/2/2020
///
#include <thread>
#include <iostream>
#include "Client.hpp"
#include "../logger.h"
#include "../Settings.hpp"
#include "../Lua System/LuaSystem.hpp"

void SendToAll(Client*c, const std::string& Data, bool Self, bool Rel);
void Respond(Client*c, const std::string& MSG, bool Rel);
void UpdatePlayers();


int TriggerLuaEvent(const std::string& Event,bool local,Lua*Caller,LuaArg* arg);
void VehicleParser(Client*c, std::string Packet){
    char Code = Packet.at(1);
    int PID = -1;
    int VID = -1;
    std::string Data = Packet.substr(3),pid,vid;
    switch(Code){ //Spawned Destroyed Switched/Moved NotFound Reset
        case 's':
            if(Data.at(0) == '0'){
                int CarID = c->GetOpenCarID();
                std::cout << c->GetName() << " CarID : " << CarID << std::endl;
                Packet = "Os:"+c->GetRole()+":"+c->GetName()+":"+std::to_string(c->GetID())+"-"+std::to_string(CarID)+Packet.substr(4);
                if(TriggerLuaEvent("onVehicleSpawn",false,nullptr,
                                   new LuaArg{{c->GetID(),CarID,Packet.substr(3)}})
                || c->GetCarCount() >= MaxCars){
                    Respond(c,Packet,true);
                    std::string Destroy = "Od:" + std::to_string(c->GetID())+"-"+std::to_string(CarID);
                    Respond(c,Destroy,true);
                }else{
                    c->AddNewCar(CarID,Packet);
                    SendToAll(nullptr, Packet,true,true);
                }
            }
            break;
        case 'c':
            SendToAll(c,Packet,false,true);
            break;
        case 'd':
            pid = Data.substr(0,Data.find('-'));
            vid = Data.substr(Data.find('-')+1);
            if(pid.find_first_not_of("0123456789") == std::string::npos && vid.find_first_not_of("0123456789") == std::string::npos){
               PID = stoi(pid);
               VID = stoi(vid);
            }
            if(PID != -1 && VID != -1 && PID == c->GetID()){
                SendToAll(nullptr,Packet,true,true);
                c->DeleteCar(VID);
            }
            break;
        case 'r':
            SendToAll(c,Packet,false,true);
            break;
        case 'm':
            break;
    }
}
void SyncVehicles(Client*c){
    Respond(c,"Sn"+c->GetName(),true);
    SendToAll(c,"JWelcome "+c->GetName()+"!",false,true);
    TriggerLuaEvent("onPlayerJoin",false,nullptr,new LuaArg{{c->GetID()}});
    for (Client*client : Clients) {
        if (client != c) {
            for(const std::pair<int,std::string>&a : client->GetAllCars()){
                Respond(c,a.second,true);
            }
        }
    }
}


extern int PPS;
void GlobalParser(Client*c, const std::string&Packet){
    if(Packet.empty())return;
    if(Packet.find("TEST")!=std::string::npos)SyncVehicles(c);
    char Code = Packet.at(0),SubCode = 0;
    if(Packet.length() > 1)SubCode = Packet.at(1);
    switch (Code) {
        case 'P':
            Respond(c, "P" + std::to_string(c->GetID()),true);
            return;
        case 'p':
            Respond(c,"p",false);
            UpdatePlayers();
            return;
        case 'O':
            if(Packet.length() > 1000) {
                std::cout << "Received data from: " << c->GetName() << " Size: " << Packet.length() << std::endl;
            }
            VehicleParser(c,Packet);
            return;
        case 'J':
            SendToAll(c,Packet,false,true);
            break;
        case 'C':
            if(TriggerLuaEvent("onChatMessage",false,nullptr,
                    new LuaArg{{c->GetID(),c->GetName(),Packet.substr(Packet.find(':',3)+1)}}))break;
            SendToAll(nullptr,Packet,true,true);
            break;
        case 'E':
            SendToAll(nullptr,Packet,true,true);
            break;
    }
    //V to Z
    if(Code <= 90 && Code >= 86){
        PPS++;
        SendToAll(c,Packet,false,false);
    }
    if(Debug)debug("Vehicle Data Received from " + c->GetName());
}
