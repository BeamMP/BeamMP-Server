///
/// Created by Anonymous275 on 4/2/2020
///
#include <thread>
#include <iostream>
#include "Client.hpp"
#include "../logger.h"
#include "../Settings.hpp"

void SendToAll(Client*c, const std::string& Data, bool Self, bool Rel);
std::string HTTP_REQUEST(const std::string& IP,int port);
void Respond(Client*c, const std::string& MSG, bool Rel);
void UpdatePlayers();

void FindAndSync(Client*c,int VehID){
    for (Client*client : Clients) {
        if (client != c){
            if(client->GetID() == VehID){ /////mark
                Respond(client,c->GetCarData(VehID),true);
            }
        }
    }
}
void VehicleParser(Client*c, std::string Packet){
    char Code = Packet.at(1);
    int ID = -1;
    std::string Data = Packet.substr(3);
    switch(Code){ //Spawned Destroyed Switched/Moved NotFound Reset
        case 's':
            if(Data.at(0) == '0'){
                if(c->GetCarCount() >= MaxCars){
                    std::string Destroy = "Od:" + std::to_string(c->GetID()); ///to revise
                    SendToAll(c,Destroy,true,true);
                    c->DeleteCar(c->GetID());
                }
                Packet = "Os:"+c->GetRole()+":"+c->GetName()+":"+std::to_string(c->GetID())+Packet.substr(4);
                c->AddNewCar(c->GetID(),Packet); ///revise later
            }
            SendToAll(nullptr,Packet,true,true);
            break;
        case 'd':
            if(Data.find_first_not_of("0123456789") == std::string::npos){
               ID = stoi(Data);
            }
            if(ID != -1 && ID == c->GetID()){
                SendToAll(nullptr,Packet,true,true);
                c->DeleteCar(c->GetID());
                ///std::cout << "Delete Requested from " << c->GetName() << std::endl;
            }
            break;
        case 'm':
            break;
        case 'n':
            if(Packet.substr(3).find_first_not_of("0123456789") == std::string::npos){
                ID = stoi(Packet.substr(3));
            }
            FindAndSync(c,ID);
            break;
        case 'r':
            SendToAll(c,Packet,false,true);
            break;
    }
}

void SyncVehicles(Client*c){
    Respond(c,"Sn"+c->GetName(),true);
    SendToAll(c,"JWelcome "+c->GetName()+"!",false,true);
    for (Client*client : Clients) {
        if (client != c) {
            Respond(c,client->GetCarData(client->GetID()),true); ///revise later
        }
    }
}

void HTTP(Client*c){
    if(!c->GetDID().empty()){
        std::string a = HTTP_REQUEST("https://beamng-mp.com/entitlement?did="+c->GetDID(),443);
        if(!a.empty()){
            int pos = a.find('"');
            c->SetRole(a.substr(pos+1,a.find('"',pos+1)-2));
            if(Debug)debug("ROLE -> " + c->GetRole() + " ID -> " + c->GetDID());
        }
    }
}

void GrabRole(Client*c){
    std::thread t1(HTTP,c);
    t1.detach();
}

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
        case 'N':
            if(SubCode == 'R'){
                c->SetName(Packet.substr(2,Packet.find(':')-2));
                c->SetDID(Packet.substr(Packet.find(':')+1));
                GrabRole(c);
            }
            std::cout << "Name : " << c->GetName() << std::endl;
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
            SendToAll(nullptr,Packet,true,true);
            break;
        case 'E':
            SendToAll(nullptr,Packet,true,true);
            break;
    }
    //V to Z
    if(Packet.length() > 1000){
        std::cout << "Received data from: " << c->GetName() << " Size: " << Packet.length() << std::endl;
    }

    if(Code <= 90 && Code >= 86)SendToAll(c,Packet,false,false);
    if(Debug)debug("Data : " + Packet);
}
