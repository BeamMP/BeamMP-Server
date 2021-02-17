#include "Client.h"

#include "CustomAssert.h"
#include <memory>

// FIXME: add debug prints

void TClient::DeleteCar(int Ident) {
    for (auto& v : mVehicleData) {
        if (v != nullptr && v->ID() == Ident) {
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
            if (v != nullptr && v->ID() == OpenID) {
                OpenID++;
                found = false;
            }
        }
    } while (!found);
    return OpenID;
}

void TClient::AddNewCar(int Ident, const std::string& Data) {
    mVehicleData.insert(std::make_unique<TVehicleData>(TVehicleData { Ident, Data }));
}

TClient::TSetOfVehicleData& TClient::GetAllCars() {
    return mVehicleData;
}

std::string TClient::GetCarData(int Ident) {
    for (auto& v : mVehicleData) {
        if (v != nullptr && v->ID() == Ident) {
            return v->Data();
        }
    }
    DeleteCar(Ident);
    return "";
}

void TClient::SetCarData(int Ident, const std::string& Data) {
    for (auto& v : mVehicleData) {
        if (v != nullptr && v->ID() == Ident) {
            v->Data() = Data;
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
    return seconds;
}
