#include "VehicleData.h"

#include <utility>
#include "Common.h"

TVehicleData::TVehicleData(int ID, std::string Data)
    : mID(ID)
    , mData(std::move(Data)) {
#ifdef DEBUG
    debug("vehicle " + std::to_string(mID) + " constructed");
#endif
}

TVehicleData::~TVehicleData() {
#ifdef DEBUG
    debug("vehicle " + std::to_string(mID) + " destroyed");
#endif
}
