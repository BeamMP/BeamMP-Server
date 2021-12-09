#include "TLuaEngine.h"

class TLuaPlugin {
public:
    TLuaPlugin(TLuaEngine& Engine, const TLuaPluginConfig& Config, const fs::path& MainFolder);
    TLuaPlugin(const TLuaPlugin&) = delete;
    TLuaPlugin& operator=(const TLuaPlugin&) = delete;
    ~TLuaPlugin() noexcept = default;

    const TLuaPluginConfig& GetConfig() const { return mConfig; }
    fs::path GetFolder() const { return mFolder; }

private:
    TLuaPluginConfig mConfig;
    TLuaEngine& mEngine;
    fs::path mFolder;
    std::string mPluginName;
    std::unordered_map<std::string, std::shared_ptr<std::string>> mFileContents;
};
