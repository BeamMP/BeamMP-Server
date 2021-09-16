#pragma once

#include "TLuaEngine.h"
#include <tuple>

namespace LuaAPI {
void Print(sol::variadic_args);
namespace MP {
    extern TLuaEngine* Engine;

    std::string GetOSName();
    std::tuple<int, int, int> GetServerVersion();
    bool TriggerClientEvent(int PlayerID, const std::string& EventName, const std::string& Data);
    inline size_t GetPlayerCount() { return Engine->Server().ClientCount(); }
    void DropPlayer(int ID, std::optional<std::string> MaybeReason);
    void SendChatMessage(int ID, const std::string& Message);
    void RemoveVehicle(int PlayerID, int VehicleID);
    void Set(int ConfigID, sol::object NewValue);
    bool IsPlayerGuest(int ID);
    bool IsPlayerConnected(int ID);
    void Sleep(size_t Ms);
}
}
