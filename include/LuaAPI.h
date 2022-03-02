#pragma once

#include "TLuaEngine.h"
#include <tuple>

namespace LuaAPI {
int PanicHandler(lua_State* State);
std::string LuaToString(const sol::object Value, size_t Indent = 1, bool QuoteStrings = false);
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
    void PrintRaw(sol::variadic_args);
    std::string JsonSerialize(const sol::table& object);
}
namespace FS {
    std::pair<bool, std::string> CreateDirectory(const std::string& Path);
    std::pair<bool, std::string> Remove(const std::string& Path);
    std::pair<bool, std::string> Rename(const std::string& Path, const std::string& NewPath);
    std::pair<bool, std::string> Copy(const std::string& Path, const std::string& NewPath);
    std::string GetFilename(const std::string& Path);
    std::string GetExtension(const std::string& Path);
    std::string GetParentFolder(const std::string& Path);
    bool Exists(const std::string& Path);
    bool IsDirectory(const std::string& Path);
    bool IsFile(const std::string& Path);
    std::string ConcatPaths(sol::variadic_args Args);
}
}
