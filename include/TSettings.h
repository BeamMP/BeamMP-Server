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

    std::unordered_map<Key, SettingsTypeVariant> SettingsMap {};

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
