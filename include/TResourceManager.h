#pragma once

#include "Common.h"

class TResourceManager {
public:
    TResourceManager();

    [[nodiscard]] size_t MaxModSize() const { return mMaxModSize; }
    [[nodiscard]] std::string FileList() const { return mFileList; }
    [[nodiscard]] std::string TrimmedList() const { return mTrimmedList; }
    [[nodiscard]] std::string FileSizes() const { return mFileSizes; }
    [[nodiscard]] int ModsLoaded() const { return mModsLoaded; }

private:
    size_t mMaxModSize = 0;
    std::string mFileSizes;
    std::string mFileList;
    std::string mTrimmedList;
    int mModsLoaded = 0;
};