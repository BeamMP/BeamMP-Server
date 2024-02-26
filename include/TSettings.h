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
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <variant>

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

    std::unordered_map<Key, SettingsTypeVariant> SettingsMap {
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
        read, // Value can be read from console
        write, // Value can be read and written to from console
        noaccess // Value is inaccessible from console (no read OR write)
    };

    using SettingsAccessControl = std::pair<
        Key, // The Key's corresponding enum encoding
        SettingsAccessMask // Console read/write permissions
        >;

    std::unordered_map<std::string, SettingsAccessControl> InputAccessMapping {
        { "Description", { General_Description, write } },
        { "Tags", { General_Tags, write } },
        { "MaxPlayers", { General_MaxPlayers, write } },
        { "Name", { General_Name, write } },
        { "Map", { General_Map, read } },
        { "AuthKey", { General_AuthKey, noaccess } },
        { "Private", { General_Private, read } },
        { "Port", { General_Port, read } },
        { "MaxCars", { General_MaxCars, write } },
        { "LogChat", { General_LogChat, read } },
        { "Resourcefolder", { General_ResourceFolder, read } },
        { "Debug", { General_Debug, noaccess } },
        { "SendErrorsShowMessage", { Misc_SendErrorsShowMessage, noaccess } },
        { "SendErrors", { Misc_SendErrors, noaccess } },
        { "ImScaredOfUpdates", { Misc_ImScaredOfUpdates, noaccess } }
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
        if (!SettingsMap.contains(key)) {
            throw std::logic_error { "Undefined key accessed in Settings::getAsString" };
        }
        return std::get<std::string>(SettingsMap.at(key));
    }

    int getAsInt(Key key) {
        if (!SettingsMap.contains(key)) {
            throw std::logic_error { "Undefined key accessed in Settings::getAsInt" };
        }
        return std::get<int>(SettingsMap.at(key));
    }

    bool getAsBool(Key key) {
        if (!SettingsMap.contains(key)) {
            throw std::logic_error { "Undefined key accessed in Settings::getAsBool" };
        }
        return std::get<bool>(SettingsMap.at(key));
    }

    SettingsTypeVariant get(Key key) {
        if (!SettingsMap.contains(key)) {
            throw std::logic_error { "Undefined setting key accessed in Settings::get" };
        }
        return SettingsMap.at(key);
    }

    void set(Key key, std::string value) {
        if (!SettingsMap.contains(key)) {
            throw std::logic_error { "Undefined setting key accessed in Settings::getAsString" };
        }
        if (!std::holds_alternative<std::string>(SettingsMap.at(key))) {
            throw std::logic_error { "Wrong value type in Settings::get: std::string" };
        }
        SettingsMap.at(key) = value;
    }

    void set(Key key, int value) {
        if (!SettingsMap.contains(key)) {
            throw std::logic_error { "Undefined setting key accessed in Settings::getAsString" };
        }
        if (!std::holds_alternative<int>(SettingsMap.at(key))) {
            throw std::logic_error { "Wrong value type in Settings::get: std::string" };
        }
        SettingsMap.at(key) = value;
    }

    void set(Key key, bool value) {
        if (!SettingsMap.contains(key)) {
            throw std::logic_error { "Undefined setting key accessed in Settings::getAsString" };
        }
        if (!std::holds_alternative<bool>(SettingsMap.at(key))) {
            throw std::logic_error { "Wrong value type in Settings::get: std::string" };
        }
        SettingsMap.at(key) = value;
    }

    const std::unordered_map<std::string, SettingsAccessControl>& getACLMap() const {
        return InputAccessMapping;
    }
    SettingsAccessControl getConsoleInputAccessMapping(const std::string& keyName) {
        if (!InputAccessMapping.contains(keyName)) {
            throw std::logic_error { "Unknown key name accessed in Settings::getConsoleInputAccessMapping" };
        } else if (InputAccessMapping.at(keyName).second == SettingsAccessMask::noaccess) {
            throw std::logic_error { "Key " + keyName + " is not accessible from within the runtime!" };
        }
        return InputAccessMapping.at(keyName);
    }

    void setConsoleInputAccessMapping(const std::string& keyName, std::string value) {
        if (!InputAccessMapping.contains(keyName)) {
            throw std::logic_error { "Unknown key name accessed in Settings::setConsoleInputAccessMapping" };
        } else if (InputAccessMapping.at(keyName).second == SettingsAccessMask::noaccess) {
            throw std::logic_error { "Key " + keyName + " is not accessible from within the runtime!" };
        } else if (InputAccessMapping.at(keyName).second == SettingsAccessMask::read) {
            throw std::logic_error { "Key " + keyName + " is not writeable from within the runtime!" };
        }

        Key key = InputAccessMapping.at(keyName).first;

        if (!std::holds_alternative<std::string>(SettingsMap.at(key))) {
            throw std::logic_error { "Wrong value type in Settings::setConsoleInputAccessMapping: expected std::string" };
        }

        SettingsMap.at(key) = value;
    }
    void setConsoleInputAccessMapping(const std::string& keyName, int value) {
        if (!InputAccessMapping.contains(keyName)) {
            throw std::logic_error { "Unknown key name accessed in Settings::setConsoleInputAccessMapping" };
        } else if (InputAccessMapping.at(keyName).second == SettingsAccessMask::noaccess) {
            throw std::logic_error { "Key " + keyName + " is not accessible from within the runtime!" };
        } else if (InputAccessMapping.at(keyName).second == SettingsAccessMask::read) {
            throw std::logic_error { "Key " + keyName + " is not writeable from within the runtime!" };
        }

        Key key = InputAccessMapping.at(keyName).first;

        if (!std::holds_alternative<int>(SettingsMap.at(key))) {
            throw std::logic_error { "Wrong value type in Settings::setConsoleInputAccessMapping: expected int" };
        }

        SettingsMap.at(key) = value;
    }
    void setConsoleInputAccessMapping(const std::string& keyName, bool value) {
        if (!InputAccessMapping.contains(keyName)) {
            throw std::logic_error { "Unknown key name accessed in Settings::setConsoleInputAccessMapping" };
        } else if (InputAccessMapping.at(keyName).second == SettingsAccessMask::noaccess) {
            throw std::logic_error { "Key " + keyName + " is not accessible from within the runtime!" };
        } else if (InputAccessMapping.at(keyName).second == SettingsAccessMask::read) {
            throw std::logic_error { "Key " + keyName + " is not writeable from within the runtime!" };
        }

        Key key = InputAccessMapping.at(keyName).first;

        if (!std::holds_alternative<bool>(SettingsMap.at(key))) {
            throw std::logic_error { "Wrong value type in Settings::setConsoleInputAccessMapping: expected bool" };
        }

        SettingsMap.at(key) = value;
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
