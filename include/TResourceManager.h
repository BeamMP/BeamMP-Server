#pragma once

#include "Common.h"
#include <optional>

using ModMap = HashMap<std::string, size_t>;

class TResourceManager {
public:
    TResourceManager();

    [[nodiscard]] size_t TotalModsSize() const { return mTotalModSize; }
    [[nodiscard]] ModMap FileMap() const { return mMods; }
    [[nodiscard]] static std::string FormatForBackend(const ModMap& mods);
    [[nodiscard]] static std::string FormatForClient(const ModMap& mods);
    [[nodiscard]] static std::optional<std::string> IsModValid(std::string& pathString, const ModMap& mods);
    [[nodiscard]] int LoadedModCount() const { return mMods.size(); }

private:
    size_t mTotalModSize = 0; // size of all mods
    ModMap mMods; // map of mod names
};