#include "VehicleData.h"

#include "Common.h"
#include <utility>

TVehicleData::TVehicleData(int ID, std::string Data)
    : mID(ID)
    , mData(std::move(Data)) {
    trace("vehicle " + std::to_string(mID) + " constructed");
}

TVehicleData::~TVehicleData() {
    trace("vehicle " + std::to_string(mID) + " destroyed");
}
