#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <unordered_set>

#include "Common.h"
#include "Compat.h"
#include "VehicleData.h"

class TServer;

#ifdef BEAMMP_WINDOWS
// for socklen_t
#include <WS2tcpip.h>
#endif // WINDOWS

struct TConnection final {
    SOCKET Socket;
    struct sockaddr SockAddr;
    socklen_t SockAddrLen;
};

class TClient final {
public:
    using TSetOfVehicleData = std::vector<TVehicleData>;

    struct TVehicleDataLockPair {
        TSetOfVehicleData* VehicleData;
        std::unique_lock<std::mutex> Lock;
    };

    explicit TClient(TServer& Server);
    TClient(const TClient&) = delete;
    TClient& operator=(const TClient&) = delete;

    void AddNewCar(int Ident, const std::string& Data);
    void SetCarData(int Ident, const std::string& Data);
    TVehicleDataLockPair GetAllCars();
    void SetName(const std::string& Name) { mName = Name; }
    void SetRoles(const std::string& Role) { mRole = Role; }
    void SetIdentifier(const std::string& key, const std::string& value) { mIdentifiers[key] = value; }
    std::string GetCarData(int Ident);
    void SetUDPAddr(sockaddr_in Addr) { mUDPAddress = Addr; }
    void SetDownSock(SOCKET CSock) { mSocket[1] = CSock; }
    void SetTCPSock(SOCKET CSock) { mSocket[0] = CSock; }
    void SetStatus(int Status) { mStatus = Status; }
    // locks
    void DeleteCar(int Ident);
    [[nodiscard]] const std::unordered_map<std::string, std::string>& GetIdentifiers() const { return mIdentifiers; }
    [[nodiscard]] sockaddr_in GetUDPAddr() const { return mUDPAddress; }
    [[nodiscard]] SOCKET GetDownSock() const { return mSocket[1]; }
    [[nodiscard]] SOCKET GetTCPSock() const { return mSocket[0]; }
    [[nodiscard]] std::string GetRoles() const { return mRole; }
    [[nodiscard]] std::string GetName() const { return mName; }
    void SetUnicycleID(int ID) { mUnicycleID = ID; }
    void SetID(int ID) { mID = ID; }
    [[nodiscard]] int GetOpenCarID() const;
    [[nodiscard]] int GetCarCount() const;
    void ClearCars();
    [[nodiscard]] int GetStatus() const { return mStatus; }
    [[nodiscard]] int GetID() const { return mID; }
    [[nodiscard]] int GetUnicycleID() const { return mUnicycleID; }
    [[nodiscard]] bool IsConnected() const { return mIsConnected; }
    [[nodiscard]] bool IsSynced() const { return mIsSynced; }
    [[nodiscard]] bool IsSyncing() const { return mIsSyncing; }
    [[nodiscard]] bool IsGuest() const { return mIsGuest; }
    void SetIsGuest(bool NewIsGuest) { mIsGuest = NewIsGuest; }
    void SetIsSynced(bool NewIsSynced) { mIsSynced = NewIsSynced; }
    void SetIsSyncing(bool NewIsSyncing) { mIsSyncing = NewIsSyncing; }
    void EnqueuePacket(const std::string& Packet);
    [[nodiscard]] std::queue<std::string>& MissedPacketQueue() { return mPacketsSync; }
    [[nodiscard]] const std::queue<std::string>& MissedPacketQueue() const { return mPacketsSync; }
    [[nodiscard]] size_t MissedPacketQueueSize() const { return mPacketsSync.size(); }
    [[nodiscard]] std::mutex& MissedPacketQueueMutex() const { return mMissedPacketsMutex; }
    void SetIsConnected(bool NewIsConnected) { mIsConnected = NewIsConnected; }
    [[nodiscard]] TServer& Server() const;
    void UpdatePingTime();
    int SecondsSinceLastPing();

private:
    void InsertVehicle(int ID, const std::string& Data);

    TServer& mServer;
    bool mIsConnected = false;
    bool mIsSynced = false;
    bool mIsSyncing = false;
    mutable std::mutex mMissedPacketsMutex;
    std::queue<std::string> mPacketsSync;
    std::unordered_map<std::string, std::string> mIdentifiers;
    bool mIsGuest = false;
    mutable std::mutex mVehicleDataMutex;
    TSetOfVehicleData mVehicleData;
    std::string mName = "Unknown Client";
    SOCKET mSocket[2] { SOCKET(0), SOCKET(0) };
    sockaddr_in mUDPAddress {}; // is this initialization OK? yes it is
    int mUnicycleID = -1;
    std::string mRole;
    std::string mDID;
    int mStatus = 0;
    int mID = -1;
    std::chrono::time_point<std::chrono::high_resolution_clock> mLastPingTime;
};

std::optional<std::weak_ptr<TClient>> GetClient(class TServer& Server, int ID);
