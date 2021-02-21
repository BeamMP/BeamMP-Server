#include "TResourceManager.h"

#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

TResourceManager::TResourceManager() {
    std::string Path = Application::Settings.Resource + "/Client";
    if (!fs::exists(Path))
        fs::create_directories(Path);
    for (const auto& entry : fs::directory_iterator(Path)) {
        auto pos = entry.path().string().find(".zip");
        if (pos != std::string::npos) {
            if (entry.path().string().length() - pos == 4) {
                mFileList += entry.path().string() + ";";
                mFileSizes += std::to_string(uint64_t(fs::file_size(entry.path()))) + ";";
                mMaxModSize += uint64_t(fs::file_size(entry.path()));
                mModsLoaded++;
            }
        }
    }
    std::replace(mFileList.begin(), mFileList.end(), '\\', '/');
    if (mModsLoaded)
        info("Loaded " + std::to_string(mModsLoaded) + " Mods");
}
