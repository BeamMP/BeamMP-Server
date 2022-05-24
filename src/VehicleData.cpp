#include "VehicleData.h"

#include "Common.h"
#include <regex>
#include <utility>

TVehicleData::TVehicleData(int ID, const std::string& Data)
    : mID(ID)
    , mData(Data) {
    try {
        std::regex Reg(R"(^[a-zA-Z0-9_\-.]+:[a-zA-Z0-9_\-.]+:[a-zA-Z0-9_\-.]+:\d+\-\d+:(\{.+\}))", std::regex::ECMAScript);
        std::smatch Match;
        std::string Result;
        if (std::regex_search(Data, Match, Reg) && Match.size() > 1) {
            Result = Match.str(1);
            mJson = json::parse(Result);
        }
    } catch (const std::exception& e) {
        beammp_debugf("Failed to parse vehicle data for vehicle {} as json, this isn't expected: {}", ID, e.what());
    }
    beammp_trace("vehicle " + std::to_string(mID) + " constructed");
}

TVehicleData::~TVehicleData() {
    beammp_trace("vehicle " + std::to_string(mID) + " destroyed");
}

const json& TVehicleData::Json() const {
    return mJson;
}
