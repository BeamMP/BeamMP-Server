#include "VehicleData.h"
#include "Common.h"

TVehicleData::TVehicleData(int ID, const std::string& Data)
    : mID(ID)
    , mData(Data) {
    debug("vehicle " + std::to_string(mID) + " constructed");
}

TVehicleData::~TVehicleData() {
    debug("vehicle " + std::to_string(mID) + " destroyed");
}
