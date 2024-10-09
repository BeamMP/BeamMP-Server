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
#include <nlohmann/json.hpp>

class TResourceManager {
public:
    TResourceManager();

    [[nodiscard]] size_t MaxModSize() const { return mMaxModSize; }
    [[nodiscard]] std::string FileList() const { return mFileList; }
    [[nodiscard]] std::string TrimmedList() const { return mTrimmedList; }
    [[nodiscard]] std::string FileSizes() const { return mFileSizes; }
    [[nodiscard]] int ModsLoaded() const { return mModsLoaded; }

    [[nodiscard]] std::string NewFileList() const;

    void RefreshFiles();

private:
    size_t mMaxModSize = 0;
    std::string mFileSizes;
    std::string mFileList;
    std::string mTrimmedList;
    int mModsLoaded = 0;

    std::mutex mModsMutex;
    nlohmann::json mMods = nlohmann::json::array();
};
