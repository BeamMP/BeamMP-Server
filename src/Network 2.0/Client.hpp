///
/// Created by Anonymous275 on 5/8/2020
///

#pragma once
#include <WS2tcpip.h>
#include <string>
#include <vector>
#include <chrono>
#include <set>

class Client {
private:
    std::set<std::pair<int,std::string>> VehicleData; //ID and Data;
    std::string Name = "Unknown Client";
    bool Connected = false;
    sockaddr_in UDPADDR;
    std::string Role;
    std::string DID; //Discord ID
    SOCKET TCPSOCK;
    int Status = 0;
    int ID = -1; //PlayerID
public:
    bool isDownloading = true;
    std::set<std::pair<int,std::string>> GetAllCars();
    void AddNewCar(int ident,const std::string& Data);
    void SetCarData(int ident,const std::string&Data);
    void SetName(const std::string& name);
    void SetRole(const std::string& role);
    void SetDID(const std::string& did);
    std::string GetCarData(int ident);
    void SetUDPAddr(sockaddr_in Addr);
    void SetTCPSock(SOCKET CSock);
    void SetConnected(bool state);
    void SetStatus(int status);
    void DeleteCar(int ident);
    sockaddr_in GetUDPAddr();
    std::string GetRole();
    std::string GetName();
    std::string GetDID();
    SOCKET GetTCPSock();
    bool isConnected();
    void SetID(int ID);
    int GetCarCount();
    int GetOpenCarID();
    int GetStatus();
    int GetID();
};

extern std::set<Client*> Clients;