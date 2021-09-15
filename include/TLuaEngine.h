#pragma once

#include "TNetwork.h"
#include "TServer.h"
#include <filesystem>
#include <sol/sol.hpp>
#include <toml11/toml.hpp>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

class TLuaPlugin;

class TLuaEngine : IThreaded {
public:
    TLuaEngine(TServer& Server, TNetwork& Network);

    void operator()() override;

private:
    void CollectPlugins();
    void InitializePlugin(const fs::path& folder);

    TNetwork& mNetwork;
    TServer& mServer;
    sol::state mL;
    std::atomic_bool mShutdown { false };
    fs::path mResourceServerPath;
    std::vector<TLuaPlugin> mLuaPlugins;
    std::unordered_map<std::string, sol::state> mLuaStates;
};
