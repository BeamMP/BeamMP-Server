#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <unordered_set>

#include "BoostAliases.h"
#include "Common.h"
#include "Compat.h"
#include "RWMutex.h"
#include "Sync.h"
#include "VehicleData.h"

class TServer;

#ifdef BEAMMP_WINDOWS
// for socklen_t
#include <WS2tcpip.h>
#endif // WINDOWS

struct TConnection final {
    ip::tcp::socket Socket;
    ip::tcp::endpoint SockAddr;
};

class TClient final {
public:
    using TSetOfVehicleData = std::vector<TVehicleData>;

    struct TVehicleDataLockPair {
        TSetOfVehicleData* VehicleData;
        std::unique_lock<std::mutex> Lock;
    };

    TClient(TServer& Server, ip::tcp::socket&& Socket);
    TClient(const TClient&) = delete;
    ~TClient();
    TClient& operator=(const TClient&) = delete;

    void AddNewCar(int Ident, const std::string& Data);
    void SetCarData(int Ident, const std::string& Data);
    void SetCarPosition(int Ident, const std::string& Data);

    void SetName(const std::string& NewName) { Name = NewName; }
    void SetRoles(const std::string& NewRole) { Role = NewRole; }
    void SetIdentifier(const std::string& key, const std::string& value);
    std::string GetCarData(int Ident);
    std::string GetCarPositionRaw(int Ident);
    void Disconnect(std::string_view Reason);
    bool IsDisconnected() const { return !TCPSocket->is_open(); }
    // locks
    void DeleteCar(int Ident);
    [[nodiscard]] int GetOpenCarID() const;
    [[nodiscard]] int GetCarCount() const;
    void ClearCars();
    void EnqueuePacket(const std::vector<uint8_t>& Packet);
    void SetIsConnected(bool NewIsConnected) { IsConnected = NewIsConnected; }
    [[nodiscard]] TServer& Server() const;
    void UpdatePingTime();
    int SecondsSinceLastPing();

    Sync<bool> IsConnected = false;
    Sync<bool> IsSynced = false;
    Sync<bool> IsSyncing = false;
    Sync<std::unordered_map<std::string, std::string>> Identifiers;
    Sync<ip::tcp::socket> TCPSocket;
    Sync<ip::tcp::socket> DownSocket;
    Sync<ip::udp::endpoint> UDPAddress {};
    Sync<int> UnicycleID = -1;
    Sync<std::string> Role;
    Sync<std::string> DID;
    Sync<int> ID = -1;
    Sync<bool> IsGuest = false;
    Sync<std::string> Name = std::string("Unknown Client");
    Sync<TSetOfVehicleData> VehicleData;
    Sync<SparseArray<std::string>> VehiclePosition;
    Sync<std::queue<std::vector<uint8_t>>> MissedPacketsQueue;
    Sync<std::chrono::time_point<std::chrono::high_resolution_clock>> LastPingTime;

private:
    void InsertVehicle(int ID, const std::string& Data);

    TServer& mServer;
};

std::optional<std::shared_ptr<TClient>> GetClient(class TServer& Server, int ID);
