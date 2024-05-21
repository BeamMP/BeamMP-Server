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

#include "Settings.h"

std::string Settings::getAsString(Key key) {
    auto map = SettingsMap.synchronize();
    if (!map->contains(key)) {
        throw std::logic_error { "Undefined key accessed in Settings::getAsString" };
    }
    return std::get<std::string>(map->at(key));
}
int Settings::getAsInt(Key key) {
    auto map = SettingsMap.synchronize();
    if (!map->contains(key)) {
        throw std::logic_error { "Undefined key accessed in Settings::getAsInt" };
    }
    return std::get<int>(map->at(key));
}

bool Settings::getAsBool(Key key) {
    auto map = SettingsMap.synchronize();
    if (!map->contains(key)) {
        throw std::logic_error { "Undefined key accessed in Settings::getAsBool" };
    }
    return std::get<bool>(map->at(key));
}

Settings::SettingsTypeVariant Settings::get(Key key) {
    auto map = SettingsMap.synchronize();
    if (!map->contains(key)) {
        throw std::logic_error { "Undefined setting key accessed in Settings::get" };
    }
    return map->at(key);
}

void Settings::set(Key key, const std::string& value) {
    auto map = SettingsMap.synchronize();
    if (!map->contains(key)) {
        throw std::logic_error { "Undefined setting key accessed in Settings::set(std::string)" };
    }
    if (!std::holds_alternative<std::string>(map->at(key))) {
        throw std::logic_error { fmt::format("Wrong value type in Settings::set(std::string): index {}", map->at(key).index()) };
    }
    map->at(key) = value;
}

const std::unordered_map<ComposedKey, Settings::SettingsAccessControl> Settings::getAccessControlMap() const {
    return *InputAccessMapping;
}

Settings::SettingsAccessControl Settings::getConsoleInputAccessMapping(const ComposedKey& keyName) {
    auto acl_map = InputAccessMapping.synchronize();
    if (!acl_map->contains(keyName)) {
        throw std::logic_error { "Unknown key name accessed in Settings::getConsoleInputAccessMapping" };
    } else if (acl_map->at(keyName).second == SettingsAccessMask::NO_ACCESS) {
        throw std::logic_error { "Setting '" + keyName.Category + " > " + keyName.Key + "' is not accessible from within the runtime!" };
    }
    return acl_map->at(keyName);
}

void Settings::setConsoleInputAccessMapping(const ComposedKey& keyName, const std::string& value) {
    auto [map, acl_map] = boost::synchronize(SettingsMap, InputAccessMapping);
    if (!acl_map->contains(keyName)) {
        throw std::logic_error { "Unknown key name accessed in Settings::setConsoleInputAccessMapping" };
    } else if (acl_map->at(keyName).second == SettingsAccessMask::NO_ACCESS) {
        throw std::logic_error { "Setting '" + keyName.Category + " > " + keyName.Key + "' is not accessible from within the runtime!" };
    } else if (acl_map->at(keyName).second == SettingsAccessMask::READ_ONLY) {
        throw std::logic_error { "Setting '" + keyName.Category + " > " + keyName.Key + "' is not writeable from within the runtime!" };
    }

    Key key = acl_map->at(keyName).first;

    if (!std::holds_alternative<std::string>(map->at(key))) {
        throw std::logic_error { "Wrong value type in Settings::setConsoleInputAccessMapping: expected std::string" };
    }

    map->at(key) = value;
}

void Settings::setConsoleInputAccessMapping(const ComposedKey& keyName, int value) {
    auto [map, acl_map] = boost::synchronize(SettingsMap, InputAccessMapping);
    if (!acl_map->contains(keyName)) {
        throw std::logic_error { "Unknown key name accessed in Settings::setConsoleInputAccessMapping" };
    } else if (acl_map->at(keyName).second == SettingsAccessMask::NO_ACCESS) {
        throw std::logic_error { "Key '" + keyName.Category + " > " + keyName.Key + "' is not accessible from within the runtime!" };
    } else if (acl_map->at(keyName).second == SettingsAccessMask::READ_ONLY) {
        throw std::logic_error { "Key '" + keyName.Category + " > " + keyName.Key + "' is not writeable from within the runtime!" };
    }

    Key key = acl_map->at(keyName).first;

    if (!std::holds_alternative<int>(map->at(key))) {
        throw std::logic_error { "Wrong value type in Settings::setConsoleInputAccessMapping: expected int" };
    }

    map->at(key) = value;
}

void Settings::setConsoleInputAccessMapping(const ComposedKey& keyName, bool value) {
    auto [map, acl_map] = boost::synchronize(SettingsMap, InputAccessMapping);
    if (!acl_map->contains(keyName)) {
        throw std::logic_error { "Unknown key name accessed in Settings::setConsoleInputAccessMapping" };
    } else if (acl_map->at(keyName).second == SettingsAccessMask::NO_ACCESS) {
        throw std::logic_error { "Key '" + keyName.Category + " > " + keyName.Key + "' is not accessible from within the runtime!" };
    } else if (acl_map->at(keyName).second == SettingsAccessMask::READ_ONLY) {
        throw std::logic_error { "Key '" + keyName.Category + " > " + keyName.Key + "' is not writeable from within the runtime!" };
    }

    Key key = acl_map->at(keyName).first;

    if (!std::holds_alternative<bool>(map->at(key))) {
        throw std::logic_error { "Wrong value type in Settings::setConsoleInputAccessMapping: expected bool" };
    }

    map->at(key) = value;
}
