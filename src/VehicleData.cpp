#include "VehicleData.h"
#include "Common.h"

TVehicleData::TVehicleData(int ID, const std::string& Data)
    : _ID(ID)
    , _Data(Data) {
    debug("vehicle " + std::to_string(_ID) + " constructed");
}

TVehicleData::~TVehicleData() {
    debug("vehicle " + std::to_string(_ID) + " destroyed");
}
