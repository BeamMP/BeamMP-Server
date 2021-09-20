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
        try {
            std::ifstream FileStream(Entry.string());
            auto Size = std::filesystem::file_size(Entry);
            auto Contents = std::make_shared<std::string>();
            Contents->resize(Size);
            FileStream.rdbuf()->sgetn(Contents->data(), Contents->size());
            mFileContents[fs::relative(Entry).string()] = Contents;
            // Execute first time
            auto Result = mEngine.EnqueueScript(mConfig.StateId, TLuaChunk(Contents, Entry.string(), MainFolder.string()));
            ResultsToCheck.emplace_back(Entry.string(), std::move(Result));
        } catch (const std::exception& e) {
            beammp_error("Error loading file \"" + Entry.string() + "\": " + e.what());
        }
    }
    for (auto& Result : ResultsToCheck) {
        Result.second->WaitUntilReady();
        if (Result.second->Error) {
            beammp_lua_error("Failed: \"" + Result.first.string() + "\": " + Result.second->ErrorMessage);
        }
    }
}
