#pragma once

#include "TNetwork.h"
#include "TServer.h"
#include <any>
#include <filesystem>
#include <initializer_list>
#include <lua.hpp>
#include <memory>
#include <mutex>
#include <queue>
#include <set>
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
    std::atomic_bool Error;
    std::string ErrorMessage;
    sol::object Result { sol::nil };
    TLuaStateId StateId;
    std::string Function;
    // TODO: Add condition_variable
    void WaitUntilReady();
};

struct TLuaPluginConfig {
    static inline const std::string FileName = "PluginConfig.toml";
    TLuaStateId StateId;
    // TODO: Add execute list
};

class TLuaEngine : IThreaded {
public:
    TLuaEngine();
    ~TLuaEngine() noexcept {
        beammp_debug("Lua Engine terminated");
    }

    void operator()() override;

    TNetwork& Network() { return *mNetwork; }
    TServer& Server() { return *mServer; }

    void SetNetwork(TNetwork* Network) { mNetwork = Network; }
    void SetServer(TServer* Server) { mServer = Server; }

    static void WaitForAll(std::vector<std::shared_ptr<TLuaResult>>& Results);
    [[nodiscard]] std::shared_ptr<TLuaResult> EnqueueScript(TLuaStateId StateID, const std::shared_ptr<std::string>& Script);
    [[nodiscard]] std::shared_ptr<TLuaResult> EnqueueFunctionCall(TLuaStateId StateID, const std::string& FunctionName, const std::initializer_list<std::any>& Args);
    void EnsureStateExists(TLuaStateId StateId, const std::string& Name, bool DontCallOnInit = false);
    void RegisterEvent(const std::string& EventName, TLuaStateId StateId, const std::string& FunctionName);
    template <typename... ArgsT>
    [[nodiscard]] std::vector<std::shared_ptr<TLuaResult>> TriggerEvent(const std::string& EventName, ArgsT&&... Args) {
        std::unique_lock Lock(mEventsMutex);
        if (mEvents.find(EventName) == mEvents.end()) {
            return {};
        }
        std::vector<std::shared_ptr<TLuaResult>> Results;
        for (const auto& Event : mEvents.at(EventName)) {
            for (const auto& Function : Event.second) {
                beammp_debug("TriggerEvent: triggering \"" + Function + "\" on \"" + Event.first + "\"");
                Results.push_back(EnqueueFunctionCall(Event.first, Function, { std::forward<ArgsT>(Args)... }));
            }
        }
        return Results;
    }
    std::set<std::string> GetEventHandlersForState(const std::string& EventName, TLuaStateId StateId);

    static constexpr const char* BeamMPFnNotFoundError = "BEAMMP_FN_NOT_FOUND";

private:
    void CollectAndInitPlugins();
    void InitializePlugin(const fs::path& Folder, const TLuaPluginConfig& Config);
    void FindAndParseConfig(const fs::path& Folder, TLuaPluginConfig& Config);

    class StateThreadData : IThreaded {
    public:
        StateThreadData(const std::string& Name, std::atomic_bool& Shutdown, TLuaStateId StateId, TLuaEngine& Engine);
        StateThreadData(const StateThreadData&) = delete;
        ~StateThreadData() noexcept { beammp_debug("\"" + mStateId + "\" destroyed"); }
        [[nodiscard]] std::shared_ptr<TLuaResult> EnqueueScript(const std::shared_ptr<std::string>& Script);
        [[nodiscard]] std::shared_ptr<TLuaResult> EnqueueFunctionCall(const std::string& FunctionName, const std::initializer_list<std::any>& Args);
        void RegisterEvent(const std::string& EventName, const std::string& FunctionName);
        void operator()() override;

    private:
        sol::table Lua_TriggerGlobalEvent(const std::string& EventName);
        sol::table Lua_TriggerLocalEvent(const std::string& EventName);
        sol::table Lua_GetPlayerIdentifiers(int ID);
        sol::table Lua_GetPlayers();

        std::string mName;
        std::atomic_bool& mShutdown;
        TLuaStateId mStateId;
        lua_State* mState;
        std::thread mThread;
        std::queue<std::pair<std::shared_ptr<std::string>, std::shared_ptr<TLuaResult>>> mStateExecuteQueue;
        std::recursive_mutex mStateExecuteQueueMutex;
        std::queue<std::tuple<std::string, std::shared_ptr<TLuaResult>, std::initializer_list<std::any>>> mStateFunctionQueue;
        std::recursive_mutex mStateFunctionQueueMutex;
        TLuaEngine* mEngine;
        sol::state_view mStateView { mState };
    };

    TNetwork* mNetwork;
    TServer* mServer;
    std::atomic_bool mShutdown { false };
    fs::path mResourceServerPath;
    std::vector<TLuaPlugin*> mLuaPlugins;
    std::unordered_map<TLuaStateId, std::unique_ptr<StateThreadData>> mLuaStates;
    std::recursive_mutex mLuaStatesMutex;
    std::unordered_map<std::string /* event name */, std::unordered_map<TLuaStateId, std::set<std::string>>> mEvents;
    std::recursive_mutex mEventsMutex;
};

//std::any TriggerLuaEvent(const std::string& Event, bool local, TLuaPlugin* Caller, std::shared_ptr<TLuaArg> arg, bool Wait);
