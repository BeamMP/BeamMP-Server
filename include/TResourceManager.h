#pragma once

#include "Common.h"

class TResourceManager {
public:
    TResourceManager();

    [[nodiscard]] uint64_t MaxModSize() const { return mMaxModSize; }
    [[nodiscard]] std::string FileList() const { return mFileList; }
    [[nodiscard]] std::string FileSizes() const { return mFileSizes; }
    [[nodiscard]] int ModsLoaded() const { return mModsLoaded; }

private:
    uint64_t mMaxModSize = 0;
    std::string mFileSizes;
    std::string mFileList;
    int mModsLoaded = 0;
};