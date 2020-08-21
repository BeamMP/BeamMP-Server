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
    for(VData* v : VehicleData){
        if(v != nullptr && v->ID == ident){
            VehicleData.erase(v);
            delete v;
            v = nullptr;
            break;
        }
    }
}
void Client::ClearCars(){
    for(VData* v : VehicleData){
        if(v != nullptr){
            delete v;
            v = nullptr;
        }
    }
    VehicleData.clear();
}
int Client::GetOpenCarID(){
    int OpenID = 0;
    bool found;
    do {
        found = true;
        for (VData*v : VehicleData) {
            if (v != nullptr && v->ID == OpenID){
                OpenID++;
                found = false;
            }
        }
    }while (!found);
    return OpenID;
}
void Client::AddNewCar(int ident,const std::string& Data){
    VehicleData.insert(new VData{ident,Data});
}

std::set<VData*> Client::GetAllCars(){
    return VehicleData;
}
std::string Client::GetCarData(int ident){
    for(VData*v : VehicleData){
        if(v != nullptr && v->ID == ident){
            return v->Data;
        }
    }
    DeleteCar(ident);
    return "";
}
void Client::SetCarData(int ident,const std::string&Data){
    for(VData*v : VehicleData){
        if(v != nullptr && v->ID == ident){
            v->Data = Data;
            return;
        }
    }
    DeleteCar(ident);
}
int Client::GetCarCount(){
    return int(VehicleData.size());
}

