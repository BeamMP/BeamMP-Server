// BeamMP, the BeamNG.drive multiplayer mod.
// Copyright (C) 2024 BeamMP Ltd., BeamMP team and contributors.
//
// BeamMP Ltd. can be contacted by electronic mail via contact@beammp.com.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

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
    std::pair<bool, std::string> TriggerClientEvent(int PlayerID, const std::string& EventName, const sol::object& Data);
    std::pair<bool, std::string> TriggerClientEventJson(int PlayerID, const std::string& EventName, const sol::table& Data);
    inline size_t GetPlayerCount() { return Engine->Server().ClientCount(); }
    std::pair<bool, std::string> DropPlayer(int ID, std::optional<std::string> MaybeReason);
    std::pair<bool, std::string> SendChatMessage(int ID, const std::string& Message);
    std::pair<bool, std::string> RemoveVehicle(int PlayerID, int VehicleID);
    void Set(int ConfigID, sol::object NewValue);
    TLuaValue Get(int ConfigID);
    bool IsPlayerGuest(int ID);
    bool IsPlayerConnected(int ID);
    void Sleep(size_t Ms);
    void PrintRaw(sol::variadic_args);
    std::string JsonEncode(const sol::table& object);
    std::string JsonDiff(const std::string& a, const std::string& b);
    std::string JsonDiffApply(const std::string& data, const std::string& patch);
    std::string JsonPrettify(const std::string& json);
    std::string JsonMinify(const std::string& json);
    std::string JsonFlatten(const std::string& json);
    std::string JsonUnflatten(const std::string& json);
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
