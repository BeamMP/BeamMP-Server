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
#include "Sync.h"
#include <cstdint>
#include <fmt/core.h>
#include <fmt/format.h>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <variant>

struct ComposedKey {
    std::string Category;
    std::string Key;

    bool operator==(const ComposedKey& rhs) const {
        return (this->Category == rhs.Category && this->Key == rhs.Key);
    }
};

template <>
struct fmt::formatter<ComposedKey> : formatter<std::string> {
    auto format(ComposedKey key, format_context& ctx) const;
};

inline auto fmt::formatter<ComposedKey>::format(ComposedKey key, fmt::format_context& ctx) const {
    std::string key_metadata = fmt::format("{}::{}", key.Category, key.Key);
    return formatter<std::string>::format(key_metadata, ctx);
}

namespace std {
template <>
class hash<ComposedKey> {
public:
    std::uint64_t operator()(const ComposedKey& key) const {
        std::hash<std::string> hash_fn;
        return hash_fn(key.Category + key.Key);
    }
};
}

struct Settings {
    using SettingsTypeVariant = std::variant<std::string, bool, int>;

    enum Key {
        // Keys that correspond to the keys set in TOML
        // Keys have their TOML section name as prefix

        // [Misc]
        Misc_SendErrorsShowMessage,
        Misc_SendErrors,
        Misc_ImScaredOfUpdates,

        // [General]
        General_Description,
        General_Tags,
        General_MaxPlayers,
        General_Name,
        General_Map,
        General_AuthKey,
        General_Private,
        General_Port,
        General_MaxCars,
        General_LogChat,
        General_ResourceFolder,
        General_Debug
    };

    Sync<std::unordered_map<Key, SettingsTypeVariant>> SettingsMap = std::unordered_map<Key, SettingsTypeVariant> {
        { General_Description, std::string("BeamMP Default Description") },
        { General_Tags, std::string("Freeroam") },
        { General_MaxPlayers, 8 },
        { General_Name, "BeamMP Server" },
        { General_Map, "/levels/gridmap_v2/info.json" },
        { General_AuthKey, "" },
        { General_Private, true },
        { General_Port, 30814 },
        { General_MaxCars, 1 },
        { General_LogChat, true },
        { General_ResourceFolder, "Resources" },
        { General_Debug, false },
        { Misc_SendErrorsShowMessage, true },
        { Misc_SendErrors, true },
        { Misc_ImScaredOfUpdates, true }
    };

    enum SettingsAccessMask {
        read, // Value can be read from console
        write, // Value can be read and written to from console
        noaccess // Value is inaccessible from console (no read OR write)
    };

    using SettingsAccessControl = std::pair<
        Key, // The Key's corresponding enum encoding
        SettingsAccessMask // Console read/write permissions
        >;

    Sync<std::unordered_map<ComposedKey, SettingsAccessControl>> InputAccessMapping = std::unordered_map<ComposedKey, SettingsAccessControl> {
        { { "General", "Description" }, { General_Description, write } },
        { { "General", "Tags" }, { General_Tags, write } },
        { { "General", "MaxPlayers" }, { General_MaxPlayers, write } },
        { { "General", "Name" }, { General_Name, write } },
        { { "General", "Map" }, { General_Map, read } },
        { { "General", "AuthKey" }, { General_AuthKey, noaccess } },
        { { "General", "Private" }, { General_Private, read } },
        { { "General", "Port" }, { General_Port, read } },
        { { "General", "MaxCars" }, { General_MaxCars, write } },
        { { "General", "LogChat" }, { General_LogChat, read } },
        { { "General", "ResourceFolder" }, { General_ResourceFolder, read } },
        { { "General", "Debug" }, { General_Debug, write } },
        { { "Misc", "SendErrorsShowMessage" }, { Misc_SendErrorsShowMessage, noaccess } },
        { { "Misc", "SendErrors" }, { Misc_SendErrors, noaccess } },
        { { "Misc", "ImScaredOfUpdates" }, { Misc_ImScaredOfUpdates, noaccess } }
    };

    /*
        std::unordered_map<std::string, Key> InputKeyMapping{
            {"Description", General_Description},
            {"Tags", General_Tags},
            {"MaxPlayers", General_MaxPlayers},
            {"Name", General_Name},
            {"Map", General_Map},
            {"AuthKey", General_AuthKey},
            {"Private", General_Private},
            {"Port", General_Port},
            {"MaxCars", General_MaxCars},
            {"LogChat", General_LogChat},
            {"Resourcefolder", General_ResourceFolder},
            {"Debug", General_Debug},
            {"SendErrorsShowMessage", Misc_SendErrorsShowMessage},
            {"SendErrors", Misc_SendErrors},
            {"ImScaredOfUpdates", Misc_ImScaredOfUpdates}
        }
    */
    std::string getAsString(Key key) {
        auto map = SettingsMap.synchronize();
        if (!map->contains(key)) {
            throw std::logic_error { "Undefined key accessed in Settings::getAsString" };
        }
        return std::get<std::string>(map->at(key));
    }

    int getAsInt(Key key) {
        auto map = SettingsMap.synchronize();
        if (!map->contains(key)) {
            throw std::logic_error { "Undefined key accessed in Settings::getAsInt" };
        }
        return std::get<int>(map->at(key));
    }

    bool getAsBool(Key key) {
        auto map = SettingsMap.synchronize();
        if (!map->contains(key)) {
            throw std::logic_error { "Undefined key accessed in Settings::getAsBool" };
        }
        return std::get<bool>(map->at(key));
    }

    SettingsTypeVariant get(Key key) {
        auto map = SettingsMap.synchronize();
        if (!map->contains(key)) {
            throw std::logic_error { "Undefined setting key accessed in Settings::get" };
        }
        return map->at(key);
    }

    void set(Key key, std::string value) {
        auto map = SettingsMap.synchronize();
        if (!map->contains(key)) {
            throw std::logic_error { "Undefined setting key accessed in Settings::set(std::string)" };
        }
        if (!std::holds_alternative<std::string>(map->at(key))) {
            throw std::logic_error { fmt::format("Wrong value type in Settings::set(std::string): index {}", map->at(key).index()) };
        }
        map->at(key) = value;
    }

    void set(Key key, int value) {
        auto map = SettingsMap.synchronize();
        if (!map->contains(key)) {
            throw std::logic_error { "Undefined setting key accessed in Settings::set(int)" };
        }
        if (!std::holds_alternative<int>(map->at(key))) {
            throw std::logic_error { fmt::format("Wrong value type in Settings::set(int): index {}", map->at(key).index()) };
        }
        map->at(key) = value;
    }

    void set(Key key, bool value) {
        auto map = SettingsMap.synchronize();
        if (!map->contains(key)) {
            throw std::logic_error { "Undefined setting key accessed in Settings::set(bool)" };
        }
        if (!std::holds_alternative<bool>(map->at(key))) {
            throw std::logic_error { fmt::format("Wrong value type in Settings::set(bool): index {}", map->at(key).index()) };
        }
        map->at(key) = value;
    }
    // Additional set overload for const char*, to avoid implicit conversions when set is
    // invoked with string literals rather than std::strings
    void set(Key key, const char* value){
        set(key, std::string(value));
    }

    const std::unordered_map<ComposedKey, SettingsAccessControl> getACLMap() const {
        return *InputAccessMapping;
    }
    SettingsAccessControl getConsoleInputAccessMapping(const ComposedKey& keyName) {
        auto acl_map = InputAccessMapping.synchronize();
        if (!acl_map->contains(keyName)) {
            throw std::logic_error { "Unknown key name accessed in Settings::getConsoleInputAccessMapping" };
        } else if (acl_map->at(keyName).second == SettingsAccessMask::noaccess) {
            throw std::logic_error { "Setting '" + keyName.Category + " > " + keyName.Key + "' is not accessible from within the runtime!" };
        }
        return acl_map->at(keyName);
    }

    void setConsoleInputAccessMapping(const ComposedKey& keyName, std::string value) {
        auto [map, acl_map] = boost::synchronize(SettingsMap, InputAccessMapping);
        if (!acl_map->contains(keyName)) {
            throw std::logic_error { "Unknown key name accessed in Settings::setConsoleInputAccessMapping" };
        } else if (acl_map->at(keyName).second == SettingsAccessMask::noaccess) {
            throw std::logic_error { "Setting '" + keyName.Category + " > " + keyName.Key + "' is not accessible from within the runtime!" };
        } else if (acl_map->at(keyName).second == SettingsAccessMask::read) {
            throw std::logic_error { "Setting '" + keyName.Category + " > " + keyName.Key + "' is not writeable from within the runtime!" };
        }

        Key key = acl_map->at(keyName).first;

        if (!std::holds_alternative<std::string>(map->at(key))) {
            throw std::logic_error { "Wrong value type in Settings::setConsoleInputAccessMapping: expected std::string" };
        }

        map->at(key) = value;
    }
    void setConsoleInputAccessMapping(const ComposedKey& keyName, int value) {
        auto [map, acl_map] = boost::synchronize(SettingsMap, InputAccessMapping);
        if (!acl_map->contains(keyName)) {
            throw std::logic_error { "Unknown key name accessed in Settings::setConsoleInputAccessMapping" };
        } else if (acl_map->at(keyName).second == SettingsAccessMask::noaccess) {
            throw std::logic_error { "Key '" + keyName.Category + " > " + keyName.Key + "' is not accessible from within the runtime!" };
        } else if (acl_map->at(keyName).second == SettingsAccessMask::read) {
            throw std::logic_error { "Key '" + keyName.Category + " > " + keyName.Key + "' is not writeable from within the runtime!" };
        }

        Key key = acl_map->at(keyName).first;

        if (!std::holds_alternative<int>(map->at(key))) {
            throw std::logic_error { "Wrong value type in Settings::setConsoleInputAccessMapping: expected int" };
        }

        map->at(key) = value;
    }
    void setConsoleInputAccessMapping(const ComposedKey& keyName, bool value) {
        auto [map, acl_map] = boost::synchronize(SettingsMap, InputAccessMapping);
        if (!acl_map->contains(keyName)) {
            throw std::logic_error { "Unknown key name accessed in Settings::setConsoleInputAccessMapping" };
        } else if (acl_map->at(keyName).second == SettingsAccessMask::noaccess) {
            throw std::logic_error { "Key '" + keyName.Category + " > " + keyName.Key + "' is not accessible from within the runtime!" };
        } else if (acl_map->at(keyName).second == SettingsAccessMask::read) {
            throw std::logic_error { "Key '" + keyName.Category + " > " + keyName.Key + "' is not writeable from within the runtime!" };
        }

        Key key = acl_map->at(keyName).first;

        if (!std::holds_alternative<bool>(map->at(key))) {
            throw std::logic_error { "Wrong value type in Settings::setConsoleInputAccessMapping: expected bool" };
        }

        map->at(key) = value;
    }
};

/*struct TSettings {


std::string ServerName { "BeamMP Server" };
std::string ServerDesc { "BeamMP Default Description" };
std::string ServerTags { "Freeroam" };
std::string Resource { "Resources" };
std::string MapName { "/levels/gridmap_v2/info.json" };
std::string Key {};
std::string Password{};
std::string SSLKeyPath { "./.ssl/HttpServer/key.pem" };
std::string SSLCertPath { "./.ssl/HttpServer/cert.pem" };
bool HTTPServerEnabled { false };
int MaxPlayers { 8 };
bool Private { true };
int MaxCars { 1 };
bool DebugModeEnabled { false };
int Port { 30814 };
std::string CustomIP {};
bool LogChat { true };
bool SendErrors { true };
bool SendErrorsMessageEnabled { true };
int HTTPServerPort { 8080 };
std::string HTTPServerIP { "127.0.0.1" };
bool HTTPServerUseSSL { false };
bool HideUpdateMessages { false };
[[nodiscard]] bool HasCustomIP() const { return !CustomIP.empty(); }
  };


}*/
