#include "Client.h"

#include "CustomAssert.h"
#include <memory>

// FIXME: add debug prints

void TClient::DeleteCar(int Ident) {
    std::unique_lock lock(mVehicleDataMutex);
    auto iter = std::find_if(mVehicleData.begin(), mVehicleData.end(), [&](auto& elem) {
        return Ident == elem.ID();
    });
    if (iter != mVehicleData.end()) {
        mVehicleData.erase(iter);
    } else {
        debug("tried to erase a vehicle that doesn't exist (not an error)");
    }
}

void TClient::ClearCars() {
    std::unique_lock lock(mVehicleDataMutex);
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
    std::unique_lock lock(mVehicleDataMutex);
    mVehicleData.emplace_back(Ident, Data);
}

TClient::TVehicleDataLockPair TClient::GetAllCars() {
    return { &mVehicleData, std::unique_lock(mVehicleDataMutex) };
}

std::string TClient::GetCarData(int Ident) {
    { // lock
        std::unique_lock lock(mVehicleDataMutex);
        for (auto& v : mVehicleData) {
            if (v.ID() == Ident) {
                return v.Data();
            }
        }
    } // unlock
    DeleteCar(Ident);
    return "";
}

void TClient::SetCarData(int Ident, const std::string& Data) {
    { // lock
        std::unique_lock lock(mVehicleDataMutex);
        for (auto& v : mVehicleData) {
            if (v.ID() == Ident) {
                v.SetData(Data);
                return;
            }
        }
    } // unlock
    DeleteCar(Ident);
}

int TClient::GetCarCount() const {
    return int(mVehicleData.size());
}

TServer& TClient::Server() const {
    return mServer;
}

void TClient::EnqueuePacket(const std::string& Packet) {
    std::unique_lock Lock(mMissedPacketsMutex);
    mPacketsSync.push(Packet);
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
