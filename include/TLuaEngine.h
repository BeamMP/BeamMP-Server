#pragma once

#include "TNetwork.h"
#include "TServer.h"
#include <filesystem>
#include <memory>
#include <mutex>
#include <queue>
#include <toml11/toml.hpp>
#include <unordered_map>
#include <vector>

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

using TLuaStateId = std::string;
namespace fs = std::filesystem;

class TLuaPlugin;

struct TLuaResult {
    std::atomic_bool Ready;
    // TODO: Add condition_variable
    sol::protected_function_result Result;
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

    TLuaResult EnqueueScript(TLuaStateId StateID, const std::shared_ptr<std::string>& Script);
    void EnsureStateExists(TLuaStateId StateId, const std::string& Name);

private:
    void CollectPlugins();
    void InitializePlugin(const fs::path& Folder, const TLuaPluginConfig& Config);
    void FindAndParseConfig(const fs::path& Folder, TLuaPluginConfig& Config);

    class StateThreadData : IThreaded {
    public:
        StateThreadData(const std::string& Name, std::atomic_bool& Shutdown);
        StateThreadData(const StateThreadData&) = delete;
        void EnqueueScript(const std::shared_ptr<std::string>& Script);
        void operator()() override;
        ~StateThreadData();

    private:
        std::string mName;
        std::atomic_bool& mShutdown;
        sol::state mState;
        std::thread mThread;
        std::queue<std::shared_ptr<std::string>> mStateExecuteQueue;
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
