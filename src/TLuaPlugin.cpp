#include "TLuaPlugin.h"
#include <chrono>
#include <functional>
#include <random>
#include <utility>

TLuaPlugin::TLuaPlugin(TLuaEngine& Engine, const TLuaPluginConfig& Config, const fs::path& MainFolder)
    : mConfig(Config)
    , mEngine(Engine)
    , mFolder(MainFolder)
    , mPluginName(MainFolder.stem().string())
    , mFileContents(0) {
    beammp_debug("Lua plugin \"" + mPluginName + "\" starting in \"" + mFolder.string() + "\"");
    std::vector<fs::path> Entries;
    for (const auto& Entry : fs::directory_iterator(mFolder)) {
        if (Entry.is_regular_file() && Entry.path().extension() == ".lua") {
            Entries.push_back(Entry);
        }
    }
    // sort alphabetically (not needed if config is used to determine call order)
    // TODO: Use config to figure out what to run in which order
    std::sort(Entries.begin(), Entries.end(), [](const fs::path& first, const fs::path& second) {
        auto firstStr = first.string();
        auto secondStr = second.string();
        std::transform(firstStr.begin(), firstStr.end(), firstStr.begin(), ::tolower);
        std::transform(secondStr.begin(), secondStr.end(), secondStr.begin(), ::tolower);
        return firstStr < secondStr;
    });
    std::vector<std::pair<fs::path, std::shared_ptr<TLuaResult>>> ResultsToCheck;
    for (const auto& Entry : Entries) {
// read in entire file
#if defined(WIN32)
        std::FILE* File = _wfopen(reinterpret_cast<const wchar_t*>(Entry.c_str()), "r");
#else
        std::FILE* File = std::fopen(reinterpret_cast<const char*>(Entry.c_str()), "r");
#endif
        if (File) {
            auto Size = std::filesystem::file_size(Entry);
            auto Contents = std::make_shared<std::string>();
            Contents->resize(Size);
            auto NRead = std::fread(Contents->data(), 1, Contents->size(), File);
            if (NRead == Contents->size()) {
                mFileContents[fs::relative(Entry).string()] = Contents;
                // Execute first time
                auto Result = mEngine.EnqueueScript(mConfig.StateId, TLuaChunk(Contents, Entry.string(), MainFolder.string()));
                ResultsToCheck.emplace_back(Entry.string(), std::move(Result));
            } else {
                beammp_error("Error while reading script file \"" + Entry.string() + "\". Did the file change while reading?");
            }
            std::fclose(File);
        } else {
            beammp_error("Could not read script file \"" + Entry.string() + "\": " + std::strerror(errno));
        }
    }
    for (auto& Result : ResultsToCheck) {
        Result.second->WaitUntilReady();
        if (Result.second->Error) {
            beammp_lua_error("Failed: \"" + Result.first.string() + "\": " + Result.second->ErrorMessage);
        }
    }
}
