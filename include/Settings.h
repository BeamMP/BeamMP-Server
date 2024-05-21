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
#include <concepts>
#include <cstdint>
#include <doctest/doctest.h>
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
        { General_Description, "BeamMP Default Description" },
        { General_Tags, "Freeroam" },
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
        READ_ONLY, // Value can be read from console
        READ_WRITE, // Value can be read and written to from console
        NO_ACCESS // Value is inaccessible from console (no read OR write)
    };

    using SettingsAccessControl = std::pair<
        Key, // The Key's corresponding enum encoding
        SettingsAccessMask // Console read/write permissions
        >;

    Sync<std::unordered_map<ComposedKey, SettingsAccessControl>> InputAccessMapping = std::unordered_map<ComposedKey, SettingsAccessControl> {
        { { "General", "Description" }, { General_Description, READ_WRITE } },
        { { "General", "Tags" }, { General_Tags, READ_WRITE } },
        { { "General", "MaxPlayers" }, { General_MaxPlayers, READ_WRITE } },
        { { "General", "Name" }, { General_Name, READ_WRITE } },
        { { "General", "Map" }, { General_Map, READ_WRITE } },
        { { "General", "AuthKey" }, { General_AuthKey, READ_WRITE } },
        { { "General", "Private" }, { General_Private, READ_ONLY } },
        { { "General", "Port" }, { General_Port, READ_ONLY } },
        { { "General", "MaxCars" }, { General_MaxCars, READ_WRITE } },
        { { "General", "LogChat" }, { General_LogChat, READ_ONLY } },
        { { "General", "ResourceFolder" }, { General_ResourceFolder, READ_ONLY } },
        { { "General", "Debug" }, { General_Debug, READ_WRITE } },
        { { "Misc", "SendErrorsShowMessage" }, { Misc_SendErrorsShowMessage, READ_WRITE } },
        { { "Misc", "SendErrors" }, { Misc_SendErrors, READ_WRITE } },
        { { "Misc", "ImScaredOfUpdates" }, { Misc_ImScaredOfUpdates, READ_WRITE } }
    };

    std::string getAsString(Key key);

    int getAsInt(Key key);

    bool getAsBool(Key key);

    SettingsTypeVariant get(Key key);

    void set(Key key, const std::string& value);

    template <typename Integer, std::enable_if_t<std::is_same_v<Integer, int>, bool> = true>
    void set(Key key, Integer value) {
        auto map = SettingsMap.synchronize();
        if (!map->contains(key)) {
            throw std::logic_error { "Undefined setting key accessed in Settings::set(int)" };
        }
        if (!std::holds_alternative<int>(map->at(key))) {
            throw std::logic_error { fmt::format("Wrong value type in Settings::set(int): index {}", map->at(key).index()) };
        }
        map->at(key) = value;
    }
    template <typename Boolean, std::enable_if_t<std::is_same_v<bool, Boolean>, bool> = true>
    void set(Key key, Boolean value) {
        auto map = SettingsMap.synchronize();
        if (!map->contains(key)) {
            throw std::logic_error { "Undefined setting key accessed in Settings::set(bool)" };
        }
        if (!std::holds_alternative<bool>(map->at(key))) {
            throw std::logic_error { fmt::format("Wrong value type in Settings::set(bool): index {}", map->at(key).index()) };
        }
        map->at(key) = value;
    }

    const std::unordered_map<ComposedKey, SettingsAccessControl> getAccessControlMap() const;
    SettingsAccessControl getConsoleInputAccessMapping(const ComposedKey& keyName);

    void setConsoleInputAccessMapping(const ComposedKey& keyName, const std::string& value);
    void setConsoleInputAccessMapping(const ComposedKey& keyName, int value);
    void setConsoleInputAccessMapping(const ComposedKey& keyName, bool value);
};

TEST_CASE("settings variant functions") {
    Settings settings;
    settings.set(Settings::General_Name, "hello, world");
    CHECK_EQ(settings.getAsString(Settings::General_Name), "hello, world");
    settings.set(Settings::General_Name, std::string("hello, world"));
    CHECK_EQ(settings.getAsString(Settings::General_Name), "hello, world");
    settings.set(Settings::General_MaxPlayers, 12);
    CHECK_EQ(settings.getAsInt(Settings::General_MaxPlayers), 12);

    CHECK_THROWS(settings.set(Settings::General_Debug, "hello, world"));
    CHECK_NOTHROW(settings.set(Settings::General_Debug, false));
}
