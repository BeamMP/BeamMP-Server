#include "Client.h"

#include <memory>

// FIXME: add debug prints

void TClient::DeleteCar(int Ident) {
    for (auto& v : _VehicleData) {
        if (v != nullptr && v->ID() == Ident) {
            _VehicleData.erase(v);
            break;
        }
    }
}

void TClient::ClearCars() {
    _VehicleData.clear();
}

int TClient::GetOpenCarID() {
    int OpenID = 0;
    bool found;
    do {
        found = true;
        for (auto& v : _VehicleData) {
            if (v != nullptr && v->ID() == OpenID) {
                OpenID++;
                found = false;
            }
        }
    } while (!found);
    return OpenID;
}

void TClient::AddNewCar(int Ident, const std::string& Data) {
    _VehicleData.insert(std::make_unique<TVehicleData>(TVehicleData { Ident, Data }));
}

TClient::TSetOfVehicleData& TClient::GetAllCars() {
    return _VehicleData;
}

std::string TClient::GetCarData(int Ident) {
    for (auto& v : _VehicleData) {
        if (v != nullptr && v->ID() == Ident) {
            return v->Data();
        }
    }
    DeleteCar(Ident);
    return "";
}

void TClient::SetCarData(int Ident, const std::string& Data) {
    for (auto& v : _VehicleData) {
        if (v != nullptr && v->ID() == Ident) {
            v->Data() = Data;
            return;
        }
    }
    DeleteCar(Ident);
}
int TClient::GetCarCount() {
    return int(_VehicleData.size());
}
