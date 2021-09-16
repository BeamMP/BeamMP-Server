#include "TLuaEngine.h"

class TLuaPlugin {
public:
    TLuaPlugin(TLuaEngine& Engine, const TLuaPluginConfig& Config);
    TLuaPlugin(const TLuaPlugin&) = delete;
    TLuaPlugin& operator=(const TLuaPlugin&) = delete;
    ~TLuaPlugin() noexcept = default;

    const TLuaPluginConfig& GetConfig() const { return mConfig; }

private:
    TLuaPluginConfig mConfig;
    TLuaEngine& mEngine;
};
