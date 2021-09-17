#include "VehicleData.h"

#include "Common.h"
#include <utility>

TVehicleData::TVehicleData(int ID, std::string Data)
    : mID(ID)
    , mData(std::move(Data)) {
    beammp_trace("vehicle " + std::to_string(mID) + " constructed");
}

TVehicleData::~TVehicleData() {
    beammp_trace("vehicle " + std::to_string(mID) + " destroyed");
}
