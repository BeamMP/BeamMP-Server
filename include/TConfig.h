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

#include "Common.h"

#include <atomic>
#include <filesystem>

#define TOML11_PRESERVE_COMMENTS_BY_DEFAULT
#include <toml.hpp> // header-only version of TOML++

namespace fs = std::filesystem;

class TConfig {
public:
    explicit TConfig(const std::string& ConfigFileName);

    [[nodiscard]] bool Failed() const { return mFailed; }

    void FlushToFile();

private:
    void CreateConfigFile();
    void ParseFromFile(std::string_view name);
    void PrintDebug();
    void TryReadValue(toml::value& Table, const std::string& Category, const std::string_view& Key, const std::string_view& Env, std::string& OutValue);
    void TryReadValue(toml::value& Table, const std::string& Category, const std::string_view& Key, const std::string_view& Env, bool& OutValue);
    void TryReadValue(toml::value& Table, const std::string& Category, const std::string_view& Key, const std::string_view& Env, int& OutValue);

    void ParseOldFormat();
    std::string TagsAsPrettyArray() const;
    bool IsDefault();
    bool mFailed { false };
    bool mDisableConfig { false };
    std::string mConfigFileName;
};
