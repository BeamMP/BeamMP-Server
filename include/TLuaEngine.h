#pragma once

#include "TNetwork.h"
#include "TServer.h"
#include <any>
#include <filesystem>
#include <lua.hpp>
#include <memory>
#include <mutex>
#include <queue>
#include <toml11/toml.hpp>
#include <unordered_map>
#include <vector>

using TLuaStateId = std::string;
namespace fs = std::filesystem;

class TLuaPlugin;

struct TLuaResult {
    std::atomic_bool Ready;
    // TODO: Add condition_variable
    std::any Result;
};

struct TLuaPluginConfig {
    static inline const std::string FileName = "PluginConfig.toml";
    TLuaStateId StateId;
    // TODO: Execute list
};

class TLuaEngine : IThreaded {
public:
    TLuaEngine(TServer& Server, TNetwork& Network);

    void operator()() override;

    [[nodiscard]] std::shared_ptr<TLuaResult> EnqueueScript(TLuaStateId StateID, const std::shared_ptr<std::string>& Script);
    void EnsureStateExists(TLuaStateId StateId, const std::string& Name);

private:
    void CollectPlugins();
    void InitializePlugin(const fs::path& Folder, const TLuaPluginConfig& Config);
    void FindAndParseConfig(const fs::path& Folder, TLuaPluginConfig& Config);

    class StateThreadData : IThreaded {
    public:
        StateThreadData(const std::string& Name, std::atomic_bool& Shutdown);
        StateThreadData(const StateThreadData&) = delete;
        [[nodiscard]] std::shared_ptr<TLuaResult> EnqueueScript(const std::shared_ptr<std::string>& Script);
        void operator()() override;
        ~StateThreadData();

    private:
        std::string mName;
        std::atomic_bool& mShutdown;
        lua_State* mState;
        std::thread mThread;
        std::queue<std::pair<std::shared_ptr<std::string>, std::shared_ptr<TLuaResult>>> mStateExecuteQueue;
        std::mutex mStateExecuteQueueMutex;
    };

    TNetwork& mNetwork;
    TServer& mServer;
    std::atomic_bool mShutdown { false };
    fs::path mResourceServerPath;
    std::vector<TLuaPlugin*> mLuaPlugins;
    std::unordered_map<TLuaStateId, std::unique_ptr<StateThreadData>> mLuaStates;
    std::mutex mLuaStatesMutex;
};

#include <any>
// DEAD CODE
struct TLuaArg {
    std::vector<std::any> args;
    void PushArgs(lua_State* State);
};
std::any TriggerLuaEvent(const std::string& Event, bool local, TLuaPlugin* Caller, std::shared_ptr<TLuaArg> arg, bool Wait);
