#pragma once

#include "TNetwork.h"
#include "TServer.h"
#include <any>
#include <condition_variable>
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
/**
 * std::variant means, that TLuaArgTypes may be one of the Types listed as template args
 */
using TLuaArgTypes = std::variant<std::string, int, sol::variadic_args, bool>;
static constexpr size_t TLuaArgTypes_String = 0;
static constexpr size_t TLuaArgTypes_Int = 1;
static constexpr size_t TLuaArgTypes_VariadicArgs = 2;
static constexpr size_t TLuaArgTypes_Bool = 3;

class TLuaPlugin;

struct TLuaResult {
    bool Ready;
    bool Error;
    std::string ErrorMessage;
    sol::object Result { sol::lua_nil };
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

struct TLuaChunk {
    TLuaChunk(std::shared_ptr<std::string> Content,
        std::string FileName,
        std::string PluginPath);
    std::shared_ptr<std::string> Content;
    std::string FileName;
    std::string PluginPath;
};

class TPluginMonitor : IThreaded {
public:
    TPluginMonitor(const fs::path& Path, TLuaEngine& Engine, std::atomic_bool& Shutdown);

    void operator()();

private:
    TLuaEngine& mEngine;
    fs::path mPath;
    std::atomic_bool& mShutdown;
    std::unordered_map<std::string, fs::file_time_type> mFileTimes;
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

    size_t GetResultsToCheckSize() {
        std::unique_lock Lock(mResultsToCheckMutex);
        return mResultsToCheck.size();
    }
    
    size_t GetLuaStateCount() {
        std::unique_lock Lock(mLuaStatesMutex);
        return mLuaStates.size();
    }
    size_t GetTimedEventsCount() {
        std::unique_lock Lock(mTimedEventsMutex);
        return mTimedEvents.size();
    }
    size_t GetRegisteredEventHandlerCount() {
        std::unique_lock Lock(mLuaEventsMutex);
        size_t LuaEventsCount = 0;
        for (const auto& State : mLuaEvents) {
            for (const auto& Events : State.second) {
                LuaEventsCount += Events.second.size();
            }
        }
        return LuaEventsCount - GetLuaStateCount();
    }

    static void WaitForAll(std::vector<std::shared_ptr<TLuaResult>>& Results,
        const std::optional<std::chrono::high_resolution_clock::duration>& Max = std::nullopt);
    void ReportErrors(const std::vector<std::shared_ptr<TLuaResult>>& Results);
    bool HasState(TLuaStateId StateId);
    [[nodiscard]] std::shared_ptr<TLuaResult> EnqueueScript(TLuaStateId StateID, const TLuaChunk& Script);
    [[nodiscard]] std::shared_ptr<TLuaResult> EnqueueFunctionCall(TLuaStateId StateID, const std::string& FunctionName, const std::vector<TLuaArgTypes>& Args);
    void EnsureStateExists(TLuaStateId StateId, const std::string& Name, bool DontCallOnInit = false);
    void RegisterEvent(const std::string& EventName, TLuaStateId StateId, const std::string& FunctionName);
    template <typename... ArgsT>
    /**
     *
     * @tparam ArgsT Template Arguments for the event (Metadata) todo: figure out what this means
     * @param EventName Name of the event
     * @param IgnoreId
     * @param Args
     * @return
     */
    [[nodiscard]] std::vector<std::shared_ptr<TLuaResult>> TriggerEvent(const std::string& EventName, TLuaStateId IgnoreId, ArgsT&&... Args) {
        std::unique_lock Lock(mLuaEventsMutex);
        beammp_event(EventName);
        if (mLuaEvents.find(EventName) == mLuaEvents.end()) { // if no event handler is defined for 'EventName', return immediately
            return {};
        }

        std::vector<std::shared_ptr<TLuaResult>> Results;
        std::vector<TLuaArgTypes> Arguments { TLuaArgTypes { std::forward<ArgsT>(Args) }... };

        for (const auto& Event : mLuaEvents.at(EventName)) {
            for (const auto& Function : Event.second) {
                if (Event.first != IgnoreId) {
                    Results.push_back(EnqueueFunctionCall(Event.first, Function, Arguments));
                }
            }
        }
        return Results; //
    }
    std::set<std::string> GetEventHandlersForState(const std::string& EventName, TLuaStateId StateId);
    void CreateEventTimer(const std::string& EventName, TLuaStateId StateId, size_t IntervalMS);
    void CancelEventTimers(const std::string& EventName, TLuaStateId StateId);
    sol::state_view GetStateForPlugin(const fs::path& PluginPath);
    TLuaStateId GetStateIDForPlugin(const fs::path& PluginPath);
    void AddResultToCheck(const std::shared_ptr<TLuaResult>& Result);

    static constexpr const char* BeamMPFnNotFoundError = "BEAMMP_FN_NOT_FOUND";

    // Debugging functions (slow)
    std::unordered_map<std::string /*event name */, std::vector<std::string> /* handlers */> Debug_GetEventsForState(TLuaStateId StateId);
    std::queue<std::pair<TLuaChunk, std::shared_ptr<TLuaResult>>> Debug_GetStateExecuteQueueForState(TLuaStateId StateId);
    std::queue<std::tuple<std::string, std::shared_ptr<TLuaResult>, std::vector<TLuaArgTypes>>> Debug_GetStateFunctionQueueForState(TLuaStateId StateId);
    std::vector<TLuaResult> Debug_GetResultsToCheckForState(TLuaStateId StateId);

private:
    void CollectAndInitPlugins();
    void InitializePlugin(const fs::path& Folder, const TLuaPluginConfig& Config);
    void FindAndParseConfig(const fs::path& Folder, TLuaPluginConfig& Config);
    size_t CalculateMemoryUsage();

    class StateThreadData : IThreaded {
    public:
        StateThreadData(const std::string& Name, std::atomic_bool& Shutdown, TLuaStateId StateId, TLuaEngine& Engine);
        StateThreadData(const StateThreadData&) = delete;
        ~StateThreadData() noexcept { beammp_debug("\"" + mStateId + "\" destroyed"); }
        [[nodiscard]] std::shared_ptr<TLuaResult> EnqueueScript(const TLuaChunk& Script);
        [[nodiscard]] std::shared_ptr<TLuaResult> EnqueueFunctionCall(const std::string& FunctionName, const std::vector<TLuaArgTypes>& Args);
        void RegisterEvent(const std::string& EventName, const std::string& FunctionName);
        void AddPath(const fs::path& Path); // to be added to path and cpath
        void operator()() override;
        sol::state_view State() { return sol::state_view(mState); }

        // Debug functions, slow
        std::queue<std::pair<TLuaChunk, std::shared_ptr<TLuaResult>>> Debug_GetStateExecuteQueue();
        std::queue<std::tuple<std::string, std::shared_ptr<TLuaResult>, std::vector<TLuaArgTypes>>> Debug_GetStateFunctionQueue();

    private:
        sol::table Lua_TriggerGlobalEvent(const std::string& EventName, sol::variadic_args EventArgs);
        sol::table Lua_TriggerLocalEvent(const std::string& EventName, sol::variadic_args EventArgs);
        sol::table Lua_GetPlayerIdentifiers(int ID);
        sol::table Lua_GetPlayers();
        std::string Lua_GetPlayerName(int ID);
        sol::table Lua_GetPlayerVehicles(int ID);
        sol::table Lua_HttpCreateConnection(const std::string& host, uint16_t port);
        int Lua_GetPlayerIDByName(const std::string& Name);

        std::string mName;
        std::atomic_bool& mShutdown;
        TLuaStateId mStateId;
        lua_State* mState;
        std::thread mThread;
        std::queue<std::pair<TLuaChunk, std::shared_ptr<TLuaResult>>> mStateExecuteQueue;
        std::recursive_mutex mStateExecuteQueueMutex;
        std::queue<std::tuple<std::string, std::shared_ptr<TLuaResult>, std::vector<TLuaArgTypes>>> mStateFunctionQueue;
        std::mutex mStateFunctionQueueMutex;
        std::condition_variable mStateFunctionQueueCond;
        TLuaEngine* mEngine;
        sol::state_view mStateView { mState };
        std::queue<fs::path> mPaths;
        std::recursive_mutex mPathsMutex;
    };

    struct TimedEvent {
        std::chrono::high_resolution_clock::duration Duration {};
        std::chrono::high_resolution_clock::time_point LastCompletion {};
        std::string EventName;
        TLuaStateId StateId;
        bool Expired();
        void Reset();
    };

    TNetwork* mNetwork;
    TServer* mServer;
    TPluginMonitor mPluginMonitor;
    std::atomic_bool mShutdown { false };
    fs::path mResourceServerPath;
    std::vector<std::shared_ptr<TLuaPlugin>> mLuaPlugins;
    std::unordered_map<TLuaStateId, std::unique_ptr<StateThreadData>> mLuaStates;
    std::recursive_mutex mLuaStatesMutex;
    std::unordered_map<std::string /* event name */, std::unordered_map<TLuaStateId, std::set<std::string>>> mLuaEvents;
    std::recursive_mutex mLuaEventsMutex;
    std::vector<TimedEvent> mTimedEvents;
    std::recursive_mutex mTimedEventsMutex;
    std::queue<std::shared_ptr<TLuaResult>> mResultsToCheck;
    std::recursive_mutex mResultsToCheckMutex;
};

// std::any TriggerLuaEvent(const std::string& Event, bool local, TLuaPlugin* Caller, std::shared_ptr<TLuaArg> arg, bool Wait);
