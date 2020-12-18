///
/// Created by Anonymous275 on 5/8/2020
///

#pragma once
#ifdef WIN32
#include <WS2tcpip.h>
#else
#include <arpa/inet.h>
#define SOCKET int
#endif
#include "CustomAssert.h"
#include <string>
#include <vector>
#include <chrono>
#include <set>
#include <algorithm>

struct VData{
    int ID = -1;
    std::string Data;
};

class Client {
private:
    std::set<std::unique_ptr<VData>> VehicleData; //ID and Data;
    std::string Name = "Unknown Client";
    SOCKET SOCK[2]{SOCKET(-1)};
    sockaddr_in UDPADDR;
    std::string Role;
    std::string DID;
    int Status = 0;
    int ID = -1;
public:
    void AddNewCar(int ident,const std::string& Data);
    void SetCarData(int ident,const std::string&Data);
    std::set<std::unique_ptr<VData>>& GetAllCars();
    void SetName(const std::string& name);
    void SetRoles(const std::string& role);
    std::string GetCarData(int ident);
    void SetUDPAddr(sockaddr_in Addr);
    void SetDownSock(SOCKET CSock);
    void SetTCPSock(SOCKET CSock);
    void SetStatus(int status);
    void DeleteCar(int ident);
    sockaddr_in GetUDPAddr();
    bool isConnected = false;
    std::string GetRoles();
    std::string GetName();
    bool isSynced = false;
    bool isGuest = false;
    SOCKET GetDownSock();
    SOCKET GetTCPSock();
    void SetID(int ID);
    int GetOpenCarID();
    int GetCarCount();
    void ClearCars();
    int GetStatus();
    int GetID();
};
struct ClientInterface{
    std::set<std::unique_ptr<Client>> Clients;
    void RemoveClient(Client*& c){
        Assert(c);
        c->ClearCars();
        auto Iter = std::find_if(Clients.begin(), Clients.end(), [&](auto& ptr) {
            return c == ptr.get();
        });
        Assert(Iter != Clients.end());
        if (Iter == Clients.end()) {
            return;
        }
        Clients.erase(Iter);
        c = nullptr;
    }
    void AddClient(Client*&& c){
        Assert(c);
        Clients.insert(std::move(std::unique_ptr<Client>(c)));
    }
    int Size(){
        return int(Clients.size());
    }
};

extern std::unique_ptr<ClientInterface> CI;
