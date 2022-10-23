#include "TResourceManager.h"

#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

TResourceManager::TResourceManager() {
    Application::SetSubsystemStatus("ResourceManager", Application::Status::Starting);
    std::string Path = Application::GetSettingString(StrResourceFolder) + "/Client";
    if (!fs::exists(Path))
        fs::create_directories(Path);
    for (const auto& entry : fs::directory_iterator(Path)) {
        std::string File(entry.path().string());
        if (auto pos = File.find(".zip"); pos != std::string::npos) {
            if (File.length() - pos == 4) {
                std::replace(File.begin(), File.end(), '\\', '/');
                mFileList += File + ';';
                if (auto i = File.find_last_of('/'); i != std::string::npos) {
                    ++i;
                    File = File.substr(i, pos - i);
                }
                mTrimmedList += "/" + fs::path(File).filename().string() + ';';
                mFileSizes += std::to_string(size_t(fs::file_size(entry.path()))) + ';';
                mMaxModSize += size_t(fs::file_size(entry.path()));
                mModsLoaded++;
            }
        }
    }

    if (mModsLoaded) {
        beammp_info("Loaded " + std::to_string(mModsLoaded) + " Mods");
    }

    Application::SetSubsystemStatus("ResourceManager", Application::Status::Good);
}
