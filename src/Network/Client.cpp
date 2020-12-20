// Copyright (c) 2019-present Anonymous275.
// BeamMP Server code is not in the public domain and is not free software.
// One must be granted explicit permission by the copyright holder in order to modify or distribute any part of the source or binaries.
// Anything else is prohibited. Modified works may not be published and have be upstreamed to the official repository.
///
/// Created by Anonymous275 on 5/8/2020
///
#include "Client.hpp"

#include <memory>

std::string Client::GetName() {
    return Name;
}
void Client::SetName(const std::string& name) {
    Name = name;
}
void Client::SetRoles(const std::string& role) {
    Role = role;
}
std::string Client::GetRoles() {
    return Role;
}
int Client::GetID() {
    return ID;
}
void Client::SetID(int id) {
    ID = id;
}
void Client::SetStatus(int state) {
    Status = state;
}
int Client::GetStatus() {
    return Status;
}
void Client::SetUDPAddr(sockaddr_in Addr) {
    UDPADDR = Addr;
}

void Client::SetDownSock(SOCKET CSock){
    SOCK[1] = CSock;
}
SOCKET Client::GetDownSock(){
    return SOCK[1];
}
sockaddr_in Client::GetUDPAddr() {
    return UDPADDR;
}
void Client::SetTCPSock(SOCKET CSock) {
    SOCK[0] = CSock;
}
SOCKET Client::GetTCPSock() {
    return SOCK[0];
}

void Client::DeleteCar(int ident) {
    for (auto& v : VehicleData) {
        if (v != nullptr && v->ID == ident) {
            VehicleData.erase(v);
            break;
        }
    }
}
void Client::ClearCars() {
    VehicleData.clear();
}
int Client::GetOpenCarID() {
    int OpenID = 0;
    bool found;
    do {
        found = true;
        for (auto& v : VehicleData) {
            if (v != nullptr && v->ID == OpenID) {
                OpenID++;
                found = false;
            }
        }
    } while (!found);
    return OpenID;
}
void Client::AddNewCar(int ident, const std::string& Data) {
    VehicleData.insert(std::make_unique<VData>(VData { ident, Data }));
}

std::set<std::unique_ptr<VData>>& Client::GetAllCars() {
    return VehicleData;
}

std::string Client::GetCarData(int ident) {
    for (auto& v : VehicleData) {
        if (v != nullptr && v->ID == ident) {
            return v->Data;
        }
    }
    DeleteCar(ident);
    return "";
}
void Client::SetCarData(int ident, const std::string& Data) {
    for (auto& v : VehicleData) {
        if (v != nullptr && v->ID == ident) {
            v->Data = Data;
            return;
        }
    }
    DeleteCar(ident);
}
int Client::GetCarCount() {
    return int(VehicleData.size());
}
