///
/// Created by Anonymous275 on 7/28/2020
///
#include "Logger.h"
#include "Security/Enc.h"
#include "Settings.h"
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

uint64_t MaxModSize = 0;
std::string FileSizes;
std::string FileList;
int ModsLoaded = 0;

void InitRes() {
    std::string Path = Resource + "/Client";
    if (!fs::exists(Path))
        fs::create_directory(Path);
    for (const auto& entry : fs::directory_iterator(Path)) {
        auto pos = entry.path().string().find(".zip");
        if (pos != std::string::npos) {
            if (entry.path().string().length() - pos == 4) {
                FileList += entry.path().string() + ";";
                FileSizes += std::to_string(uint64_t(fs::file_size(entry.path()))) + ";";
                MaxModSize += uint64_t(fs::file_size(entry.path()));
                ModsLoaded++;
            }
        }
    }
    std::replace(FileList.begin(), FileList.end(), '\\', '/');
    if(ModsLoaded)info("Loaded " + std::to_string(ModsLoaded) + " Mods");
}
