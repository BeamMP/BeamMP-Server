///
/// Created by Anonymous275 on 5/8/2020
///
#include "Client.hpp"

std::string Client::GetName(){
    return Name;
}
void Client::SetName(const std::string& name){
    Name = name;
}
void Client::SetDID(const std::string& did){
    DID = did;
}
void Client::SetConnected(bool state){
    Connected = state;
}
bool Client::isConnected(){
    return Connected;
}
std::string Client::GetDID(){
    return DID;
}
void Client::SetRole(const std::string& role){
    Role = role;
}
std::string Client::GetRole(){
    return Role;
}
int Client::GetID(){
    return ID;
}
void Client::SetID(int id){
    ID = id;
}
void Client::SetStatus(int state){
    Status = state;
}
int Client::GetStatus(){
    return Status;
}
void Client::SetUDPAddr(sockaddr_in Addr){
    UDPADDR = Addr;
}
sockaddr_in Client::GetUDPAddr(){
    return UDPADDR;
}
void Client::SetTCPSock(SOCKET CSock) {
    TCPSOCK = CSock;
}
SOCKET Client::GetTCPSock(){
    return TCPSOCK;
}
void Client::DeleteCar(int ident){
    for(const std::pair<int,std::string>& a : VehicleData){
        if(a.first == ident){
            VehicleData.erase(a);
            break;
        }
    }
}
int Client::GetOpenCarID(){
    int OpenID = 0;
    bool found;
    do {
        found = true;
        for (const std::pair<int, std::string> &a : VehicleData) {
            if (a.first == OpenID){
                OpenID++;
                found = false;
            }
        }
    }while (!found);
    return OpenID;
}
void Client::AddNewCar(int ident,const std::string& Data){
    VehicleData.insert(std::make_pair(ident,Data));
}

std::set<std::pair<int,std::string>> Client::GetAllCars(){
    return VehicleData;
}
std::string Client::GetCarData(int ident){
    for(const std::pair<int,std::string>& a : VehicleData){
        if(a.first == ident){
            return a.second;
        }
    }
    DeleteCar(ident);
    return "";
}
void Client::SetCarData(int ident,const std::string&Data){
    for(const std::pair<int,std::string>& a : VehicleData){
        if(a.first == ident){
            VehicleData.erase(a);
            VehicleData.insert(std::make_pair(ident,Data));
            return;
        }
    }
    DeleteCar(ident);
}
int Client::GetCarCount(){
    return VehicleData.size();
}