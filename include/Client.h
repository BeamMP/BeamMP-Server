#pragma once

#include <memory>
#include <string>
#include <unordered_set>

#include "Common.h"
#include "Compat.h"
#include "VehicleData.h"

class TClient final {
public:
    using TSetOfVehicleData = std::unordered_set<std::unique_ptr<TVehicleData>>;

    void AddNewCar(int Ident, const std::string& Data);
    void SetCarData(int Ident, const std::string& Data);
    TSetOfVehicleData& GetAllCars();
    void SetName(const std::string& Name) { _Name = Name; }
    void SetRoles(const std::string& Role) { _Role = Role; }
    std::string GetCarData(int Ident);
    void SetUDPAddr(sockaddr_in Addr) { _UDPAddress = Addr; }
    void SetDownSock(SOCKET CSock) { _Socket[1] = CSock; }
    void SetTCPSock(SOCKET CSock) { _Socket[0] = CSock; }
    void SetStatus(int Status) { _Status = Status; }
    void DeleteCar(int Ident);
    sockaddr_in GetUDPAddr() { return _UDPAddress; }
    std::string GetRoles() { return _Role; }
    std::string GetName() { return _Name; }
    SOCKET GetDownSock() { return _Socket[1]; }
    SOCKET GetTCPSock() { return _Socket[0]; }
    void SetID(int ID) { _ID = ID; }
    int GetOpenCarID();
    int GetCarCount();
    void ClearCars();
    int GetStatus() { return _Status; }
    int GetID() { return _ID; }
    bool IsConnected() const { return _IsConnected; }
    bool IsSynced() const { return _IsSynced; }
    bool IsGuest() const { return _IsGuest; }

private:
    bool _IsConnected = false;
    bool _IsSynced = false;
    bool _IsGuest = false;
    TSetOfVehicleData _VehicleData;
    std::string _Name = "Unknown Client";
    SOCKET _Socket[2] { SOCKET(-1) };
    sockaddr_in _UDPAddress;
    std::string _Role;
    std::string _DID;
    int _Status = 0;
    int _ID = -1;
};
