///
/// Created by Anonymous275 on 8/1/2020
///
#include "Lua/LuaSystem.hpp"
#include "Security/Enc.h"
#include "Client.hpp"
#include "Settings.h"
#include "Network.h"
#include "Logger.h"
int OpenID(){
    int ID = 0;
    bool found;
    do {
        found = true;
        for (Client *c : CI->Clients){
            if(c != nullptr){
               if(c->GetID() == ID){
                    found = false;
                    ID++;
               }
            }
        }
    }while (!found);
    return ID;
}
void Respond(Client*c, const std::string& MSG, bool Rel){
    Assert(c);
    char C = MSG.at(0);
    if(Rel || C == 'W' || C == 'Y' || C == 'V' || C == 'E'){
        if(C == 'O' || C == 'T' || MSG.length() > 1000)SendLarge(c,MSG);
        else TCPSend(c,MSG);
    }else UDPSend(c,MSG);

}
void SendToAll(Client*c, const std::string& Data, bool Self, bool Rel){
    if (!Self)Assert(c);
    char C = Data.at(0);
    for(Client*client : CI->Clients){
        if(client != nullptr) {
            if (Self || client != c) {
                if (client->isSynced) {
                    if (Rel || C == 'W' || C == 'Y' || C == 'V' || C == 'E') {
                        if (C == 'O' || C == 'T' ||
                        Data.length() > 1000)SendLarge(client, Data);
                        else TCPSend(client, Data);
                    } else UDPSend(client, Data);
                }
            }
        }
    }
}
void UpdatePlayers(){
    std::string Packet = Sec("Ss") + std::to_string(CI->Size())+"/"+std::to_string(MaxPlayers) + ":";
    for (Client*c : CI->Clients) {
        if(c != nullptr)Packet += c->GetName() + ",";
    }
    Packet = Packet.substr(0,Packet.length()-1);
    SendToAll(nullptr, Packet,true,true);
}
void OnDisconnect(Client*c,bool kicked){

    Assert(c);
    info(c->GetName() + Sec(" Connection Terminated"));
    if(c == nullptr)return;
    std::string Packet;
    for(VData*v : c->GetAllCars()){
        if(v != nullptr) {
            Packet = "Od:" + std::to_string(c->GetID()) + "-" + std::to_string(v->ID);
            SendToAll(c, Packet, false, true);
        }
    }
    if(kicked)Packet = Sec("L")+c->GetName()+Sec(" was kicked!");
    Packet = Sec("L")+c->GetName()+Sec(" Left the server!");
    SendToAll(c, Packet,false,true);
    Packet.clear();
    TriggerLuaEvent(Sec("onPlayerDisconnect"),false,nullptr,std::unique_ptr<LuaArg>(new LuaArg{{c->GetID()}}),false);
    CI->RemoveClient(c); ///Removes the Client from existence
}
void OnConnect(Client*c){
    Assert(c);
    info(Sec("Client connected"));
    c->SetID(OpenID());
    info(Sec("Assigned ID ") + std::to_string(c->GetID()) + Sec(" to ") + c->GetName());
    TriggerLuaEvent(Sec("onPlayerConnecting"),false,nullptr,std::unique_ptr<LuaArg>(new LuaArg{{c->GetID()}}),false);
    SyncResources(c);
    if(c->GetStatus() < 0)return;
    Respond(c,"M"+MapName,true); //Send the Map on connect
    info(c->GetName() + Sec(" : Connected"));
    TriggerLuaEvent(Sec("onPlayerJoining"),false,nullptr,std::unique_ptr<LuaArg>(new LuaArg{{c->GetID()}}),false);
}
