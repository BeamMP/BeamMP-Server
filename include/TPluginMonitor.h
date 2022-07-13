#pragma once

#include "Common.h"
#include "IThreaded.h"

#include <atomic>
#include <memory>
#include <unordered_map>

class TLuaEngine;

class TPluginMonitor : IThreaded, public std::enable_shared_from_this<TPluginMonitor> {
public:
    TPluginMonitor(const fs::path& Path, std::shared_ptr<TLuaEngine> Engine);

    void operator()();

private:
    std::shared_ptr<TLuaEngine> mEngine;
    fs::path mPath;
    std::atomic_bool mShutdown { false };
    std::unordered_map<std::string, fs::file_time_type> mFileTimes;
};
