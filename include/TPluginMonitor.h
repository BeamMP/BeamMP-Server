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
#include "IThreaded.h"

#include <atomic>
#include <memory>
#include <unordered_map>

class TLuaEngine;

class TPluginMonitor : IThreaded, public std::enable_shared_from_this<TPluginMonitor> {
public:
    TPluginMonitor(const fs::path& Path, std::shared_ptr<TLuaEngine> Engine);

    void operator()();

private:
    std::shared_ptr<TLuaEngine> mEngine;
    fs::path mPath;
    std::unordered_map<std::string, fs::file_time_type> mFileTimes;
};
