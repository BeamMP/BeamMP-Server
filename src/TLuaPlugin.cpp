#include "TLuaPlugin.h"
#include <chrono>
#include <functional>
#include <random>
#include <utility>

TLuaPlugin::TLuaPlugin(TLuaEngine& Engine, const TLuaPluginConfig& Config)
    : mConfig(Config)
    , mEngine(Engine) {
}
