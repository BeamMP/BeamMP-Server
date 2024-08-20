// BeamMP, the BeamNG.drive multiplayer mod.
// Copyright (C) 2024 BeamMP Ltd., BeamMP team and contributors.
//
// BeamMP Ltd. can be contacted by electronic mail via contact@beammp.com.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

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
    TVehicleDataLockPair GetAllCars();
    void SetName(const std::string& Name) { mName = Name; }
    void SetRoles(const std::string& Role) { mRole = Role; }
    void SetIdentifier(const std::string& key, const std::string& value) { mIdentifiers[key] = value; }
    std::string GetCarData(int Ident);
    std::string GetCarPositionRaw(int Ident);
    void SetUDPAddr(const ip::udp::endpoint& Addr) { mUDPAddress = Addr; }
    void SetDownSock(ip::tcp::socket&& CSock) { mDownSocket = std::move(CSock); }
    void SetTCPSock(ip::tcp::socket&& CSock) { mSocket = std::move(CSock); }
    void Disconnect(std::string_view Reason);
    bool IsDisconnected() const { return !mSocket.is_open(); }
    // locks
    void DeleteCar(int Ident);
    [[nodiscard]] const std::unordered_map<std::string, std::string>& GetIdentifiers() const { return mIdentifiers; }
    [[nodiscard]] const ip::udp::endpoint& GetUDPAddr() const { return mUDPAddress; }
    [[nodiscard]] ip::udp::endpoint& GetUDPAddr() { return mUDPAddress; }
    [[nodiscard]] ip::tcp::socket& GetDownSock() { return mDownSocket; }
    [[nodiscard]] const ip::tcp::socket& GetDownSock() const { return mDownSocket; }
    [[nodiscard]] ip::tcp::socket& GetTCPSock() { return mSocket; }
    [[nodiscard]] const ip::tcp::socket& GetTCPSock() const { return mSocket; }
    [[nodiscard]] std::string GetRoles() const { return mRole; }
    [[nodiscard]] std::string GetName() const { return mName; }
    void SetUnicycleID(int ID) { mUnicycleID = ID; }
    void SetID(int ID) { mID = ID; }
    [[nodiscard]] int GetOpenCarID() const;
    [[nodiscard]] int GetCarCount() const;
    void ClearCars();
    [[nodiscard]] int GetID() const { return mID; }
    [[nodiscard]] int GetUnicycleID() const { return mUnicycleID; }
    [[nodiscard]] bool IsUDPConnected() const { return mIsUDPConnected; }
    [[nodiscard]] bool IsSynced() const { return mIsSynced; }
    [[nodiscard]] bool IsSyncing() const { return mIsSyncing; }
    [[nodiscard]] bool IsGuest() const { return mIsGuest; }
    void SetIsGuest(bool NewIsGuest) { mIsGuest = NewIsGuest; }
    void SetIsSynced(bool NewIsSynced) { mIsSynced = NewIsSynced; }
    void SetIsSyncing(bool NewIsSyncing) { mIsSyncing = NewIsSyncing; }
    void EnqueuePacket(const std::vector<uint8_t>& Packet);
    [[nodiscard]] std::queue<std::vector<uint8_t>>& MissedPacketQueue() { return mPacketsSync; }
    [[nodiscard]] const std::queue<std::vector<uint8_t>>& MissedPacketQueue() const { return mPacketsSync; }
    [[nodiscard]] size_t MissedPacketQueueSize() const { return mPacketsSync.size(); }
    [[nodiscard]] std::mutex& MissedPacketQueueMutex() const { return mMissedPacketsMutex; }
    void SetIsUDPConnected(bool NewIsConnected) { mIsUDPConnected = NewIsConnected; }
    [[nodiscard]] TServer& Server() const;
    void UpdatePingTime();
    int SecondsSinceLastPing();

private:
    void InsertVehicle(int ID, const std::string& Data);

    TServer& mServer;
    bool mIsUDPConnected = false;
    bool mIsSynced = false;
    bool mIsSyncing = false;
    mutable std::mutex mMissedPacketsMutex;
    std::queue<std::vector<uint8_t>> mPacketsSync;
    std::unordered_map<std::string, std::string> mIdentifiers;
    bool mIsGuest = false;
    mutable std::mutex mVehicleDataMutex;
    mutable std::mutex mVehiclePositionMutex;
    TSetOfVehicleData mVehicleData;
    SparseArray<std::string> mVehiclePosition;
    std::string mName = "Unknown Client";
    ip::tcp::socket mSocket;
    ip::tcp::socket mDownSocket;
    ip::udp::endpoint mUDPAddress {};
    int mUnicycleID = -1;
    std::string mRole;
    std::string mDID;
    int mID = -1;
    std::chrono::time_point<std::chrono::high_resolution_clock> mLastPingTime = std::chrono::high_resolution_clock::now();
};

std::optional<std::weak_ptr<TClient>> GetClient(class TServer& Server, int ID);
