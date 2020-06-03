///
/// Created by Anonymous275 on 2/4/2020.
///
#include "Client.hpp"
#include "../logger.h"
#include "../Settings.hpp"
#include "../Lua System/LuaSystem.hpp"

void UDPSend(Client*c,const std::string&Data);
void TCPSend(Client*c,const std::string&Data);

int OpenID(){
    int ID = 0;
    bool found;
    do {
        found = true;
        for (Client *c : Clients) {
            if(c->GetID() == ID){
                found = false;
                ID++;
            }
        }
    }while (!found);
    return ID;
}
void SendLarge(Client*c,const std::string&Data);
void Respond(Client*c, const std::string& MSG, bool Rel){
    char C = MSG.at(0);
    if(Rel){
        if(C == 'O' || C == 'T' || MSG.length() > 1000)SendLarge(c,MSG);
        else TCPSend(c,MSG);
    }else UDPSend(c,MSG);
}

void SendToAll(Client*c, const std::string& Data, bool Self, bool Rel){
    char C = Data.at(0);
    for(Client*client : Clients){
        if(Self || client != c){
            if(Rel){
                if(C == 'O' || C == 'T' || Data.length() > 1000)SendLarge(client,Data);
                else TCPSend(client,Data);
            }
            else UDPSend(client,Data);
        }
    }
}

void UpdatePlayers(){
    std::string Packet = "Ss" + std::to_string(Clients.size())+"/"+std::to_string(MaxPlayers) + ":";
    for (Client*c : Clients) {
        Packet += c->GetName() + ",";
    }
    Packet = Packet.substr(0,Packet.length()-1);
    SendToAll(nullptr, Packet,true,true);
}

void OnDisconnect(Client*c,bool kicked){
    std::string Packet;
    for(const std::pair<int,std::string>&a : c->GetAllCars()){
        Packet = "Od:" + std::to_string(c->GetID()) + "-" + std::to_string(a.first);
        SendToAll(c, Packet,false,true);
    }
    if(kicked)Packet = "L"+c->GetName()+" was kicked!";
    Packet = "L"+c->GetName()+" Left the server!";
    SendToAll(c, Packet,false,true);
    Packet.clear();
    Clients.erase(c); ///Removes the Client from existence
}
int TriggerLuaEvent(const std::string& Event,bool local,Lua*Caller);
void OnConnect(Client*c){
    c->SetID(OpenID());
    std::cout << "New Client Created! ID : " << c->GetID() << std::endl;
    Respond(c,"NR",true);
    Respond(c,"M"+MapName,true); //Send the Map on connect
    TriggerLuaEvent("onPlayerJoining",false,nullptr);
}
