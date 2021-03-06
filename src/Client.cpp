#include "Client.h"

#include "CustomAssert.h"
#include <memory>

// FIXME: add debug prints

void TClient::DeleteCar(int Ident) {
    for (auto& v : mVehicleData) {
        if (v.ID() == Ident) {
            mVehicleData.erase(v);
            break;
        }
    }
}

void TClient::ClearCars() {
    mVehicleData.clear();
}

int TClient::GetOpenCarID() const {
    int OpenID = 0;
    bool found;
    do {
        found = true;
        for (auto& v : mVehicleData) {
            if (v.ID() == OpenID) {
                OpenID++;
                found = false;
            }
        }
    } while (!found);
    return OpenID;
}

void TClient::AddNewCar(int Ident, const std::string& Data) {
    mVehicleData.insert(TVehicleData(Ident, Data));
}

TClient::TVehicleDataLockPair TClient::GetAllCars() {
    return { mVehicleData, std::unique_lock(mVehicleDataMutex) };
}

std::string TClient::GetCarData(int Ident) {
    for (auto& v : mVehicleData) {
        if (v.ID() == Ident) {
            return v.Data();
        }
    }
    DeleteCar(Ident);
    return "";
}

void TClient::SetCarData(int Ident, const std::string& Data) {
    for (auto& v : mVehicleData) {
        if (v.ID() == Ident) {
            v.Data() = Data;
            return;
        }
    }
    DeleteCar(Ident);
}

int TClient::GetCarCount() const {
    return int(mVehicleData.size());
}

TServer& TClient::Server() const {
    return mServer;
}

void TClient::EnqueueMissedPacketDuringSyncing(const std::string& Packet){
    std::unique_lock Lock(mMissedPacketsMutex);
    mMissedPacketsDuringSyncing.push(Packet);
}

TClient::TClient(TServer& Server)
    : mServer(Server)
    , mLastPingTime(std::chrono::high_resolution_clock::now()) {
}

void TClient::UpdatePingTime() {
    mLastPingTime = std::chrono::high_resolution_clock::now();
}
int TClient::SecondsSinceLastPing() {
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::high_resolution_clock::now() - mLastPingTime)
                       .count();
    return int(seconds);
}
