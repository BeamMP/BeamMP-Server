#pragma once

#include "Common.h"
#include "IThreaded.h"
#include "Network.h"
#include <any>
#include <boost/thread/scoped_thread.hpp>
#include <condition_variable>
#include <filesystem>
#include <initializer_list>
#include <list>
#include <lua.hpp>
#include <memory>
#include <mutex>
#include <queue>
#include <random>
#include <set>
#include <toml.hpp>
#include <unordered_map>
#include <vector>

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

using TLuaStateId = std::string;
namespace fs = std::filesystem;
/**
 * std::variant means, that TLuaArgTypes may be one of the Types listed as template args
 */
using TLuaArgTypes = std::variant<std::string, int, sol::variadic_args, bool, std::unordered_map<std::string, std::string>>;
static constexpr size_t TLuaArgTypes_String = 0;
static constexpr size_t TLuaArgTypes_Int = 1;
static constexpr size_t TLuaArgTypes_VariadicArgs = 2;
static constexpr size_t TLuaArgTypes_Bool = 3;
static constexpr size_t TLuaArgTypes_StringStringMap = 4;

class TLuaPlugin;

struct TLuaResult {
    bool Ready;
    bool Error;
    std::string ErrorMessage;
    sol::object Result { sol::lua_nil };
    TLuaStateId StateId;
    std::string Function;
    std::shared_ptr<std::mutex> ReadyMutex {
        std::make_shared<std::mutex>()
    };
    std::shared_ptr<std::condition_variable> ReadyCondition {
        std::make_shared<std::condition_variable>()
    };

    void MarkAsReady();
    void WaitUntilReady();
};

struct TLuaPluginConfig {
    static inline const std::string FileName = "PluginConfig.toml";
    TLuaStateId StateId;
    // TODO: Add execute list
    // TODO: Build a better toml serializer, or some way to do this in an easier way
};

struct TLuaChunk {
    TLuaChunk(std::shared_ptr<std::string> Content,
        std::string FileName,
        std::string PluginPath);
    std::shared_ptr<std::string> Content;
    std::string FileName;
    std::string PluginPath;
};

class TLuaEngine : public std::enable_shared_from_this<TLuaEngine> {
public:
    enum CallStrategy : int {
        BestEffort,
        Precise,
    };

    struct QueuedFunction {
        std::string FunctionName;
        std::shared_ptr<TLuaResult> Result;
        std::vector<TLuaArgTypes> Args;
        std::string EventName; // optional, may be empty
    };

    TLuaEngine();
    virtual ~TLuaEngine() noexcept {
        beammp_debug("Lua Engine terminated");
    }

    void Start();

    std::shared_ptr<::Network> Network() { return mNetwork; }

    void SetNetwork(const std::shared_ptr<::Network>& network) { mNetwork = network; }

    size_t GetResultsToCheckSize() {
        std::unique_lock Lock(mResultsToCheckMutex);
        return mResultsToCheck.size();
    }

    size_t GetLuaStateCount() {
        std::unique_lock Lock(mLuaStatesMutex);
        return mLuaStates.size();
    }
    std::vector<std::string> GetLuaStateNames() {
        std::vector<std::string> names {};
        for (auto const& [stateId, _] : mLuaStates) {
            names.push_back(stateId);
        }
        return names;
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
    /**
     *
     * @tparam ArgsT Template Arguments for the event (Metadata) todo: figure out what this means
     * @param EventName Name of the event
     * @param IgnoreId
     * @param Args
     * @return
     */
    template <typename... ArgsT>
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
    template <typename... ArgsT>
    [[nodiscard]] std::vector<std::shared_ptr<TLuaResult>> TriggerLocalEvent(const TLuaStateId& StateId, const std::string& EventName, ArgsT&&... Args) {
        std::unique_lock Lock(mLuaEventsMutex);
        beammp_event(EventName + " in '" + StateId + "'");
        if (mLuaEvents.find(EventName) == mLuaEvents.end()) { // if no event handler is defined for 'EventName', return immediately
            return {};
        }
        std::vector<std::shared_ptr<TLuaResult>> Results;
        std::vector<TLuaArgTypes> Arguments { TLuaArgTypes { std::forward<ArgsT>(Args) }... };
        const auto Handlers = GetEventHandlersForState(EventName, StateId);
        for (const auto& Handler : Handlers) {
            Results.push_back(EnqueueFunctionCall(StateId, Handler, Arguments));
        }
        return Results;
    }
    std::set<std::string> GetEventHandlersForState(const std::string& EventName, TLuaStateId StateId);
    void CreateEventTimer(const std::string& EventName, TLuaStateId StateId, size_t IntervalMS, CallStrategy Strategy);
    void CancelEventTimers(const std::string& EventName, TLuaStateId StateId);
    sol::state_view GetStateForPlugin(const fs::path& PluginPath);
    TLuaStateId GetStateIDForPlugin(const fs::path& PluginPath);
    void AddResultToCheck(const std::shared_ptr<TLuaResult>& Result);

    static constexpr const char* BeamMPFnNotFoundError = "BEAMMP_FN_NOT_FOUND";

    std::vector<std::string> GetStateGlobalKeysForState(TLuaStateId StateId);
    std::vector<std::string> GetStateTableKeysForState(TLuaStateId StateId, std::vector<std::string> keys);

    // Debugging functions (slow)
    std::unordered_map<std::string /*event name */, std::vector<std::string> /* handlers */> Debug_GetEventsForState(TLuaStateId StateId);
    std::queue<std::pair<TLuaChunk, std::shared_ptr<TLuaResult>>> Debug_GetStateExecuteQueueForState(TLuaStateId StateId);
    std::vector<QueuedFunction> Debug_GetStateFunctionQueueForState(TLuaStateId StateId);
    std::vector<TLuaResult> Debug_GetResultsToCheckForState(TLuaStateId StateId);

private:
    void CollectAndInitPlugins();
    void InitializePlugin(const fs::path& Folder, const TLuaPluginConfig& Config);
    void FindAndParseConfig(const fs::path& Folder, TLuaPluginConfig& Config);
    size_t CalculateMemoryUsage();

    class StateThreadData {
    public:
        StateThreadData(const std::string& Name, TLuaStateId StateId, TLuaEngine& Engine);
        StateThreadData(const StateThreadData&) = delete;
        virtual ~StateThreadData() noexcept { beammp_debug("\"" + mStateId + "\" destroyed"); }
        [[nodiscard]] std::shared_ptr<TLuaResult> EnqueueScript(const TLuaChunk& Script);
        [[nodiscard]] std::shared_ptr<TLuaResult> EnqueueFunctionCall(const std::string& FunctionName, const std::vector<TLuaArgTypes>& Args);
        [[nodiscard]] std::shared_ptr<TLuaResult> EnqueueFunctionCallFromCustomEvent(const std::string& FunctionName, const std::vector<TLuaArgTypes>& Args, const std::string& EventName, CallStrategy Strategy);
        void RegisterEvent(const std::string& EventName, const std::string& FunctionName);
        void AddPath(const fs::path& Path); // to be added to path and cpath
        void Start();
        sol::state_view State() { return sol::state_view(mState); }

        std::vector<std::string> GetStateGlobalKeys();
        std::vector<std::string> GetStateTableKeys(const std::vector<std::string>& keys);

        // Debug functions, slow
        std::queue<std::pair<TLuaChunk, std::shared_ptr<TLuaResult>>> Debug_GetStateExecuteQueue();
        std::vector<TLuaEngine::QueuedFunction> Debug_GetStateFunctionQueue();

        sol::table Lua_JsonDecode(const std::string& str);

    private:
        sol::table Lua_TriggerGlobalEvent(const std::string& EventName, sol::variadic_args EventArgs);
        sol::table Lua_TriggerLocalEvent(const std::string& EventName, sol::variadic_args EventArgs);
        sol::table Lua_GetPlayerIdentifiers(int ID);
        sol::table Lua_GetPlayers();
        std::string Lua_GetPlayerName(int ID);
        sol::table Lua_GetPlayerVehicles(int ID);
        std::pair<sol::table, std::string> Lua_GetVehicleStatus(int VID);
        sol::table Lua_HttpCreateConnection(const std::string& host, uint16_t port);
        int Lua_GetPlayerIDByName(const std::string& Name);
        sol::table Lua_FS_ListFiles(const std::string& Path);
        sol::table Lua_FS_ListDirectories(const std::string& Path);

        std::string mName;
        TLuaStateId mStateId;
        lua_State* mState;
        std::queue<std::pair<TLuaChunk, std::shared_ptr<TLuaResult>>> mStateExecuteQueue;
        std::recursive_mutex mStateExecuteQueueMutex;
        std::vector<QueuedFunction> mStateFunctionQueue;
        std::mutex mStateFunctionQueueMutex;
        std::condition_variable mStateFunctionQueueCond;
        TLuaEngine* mEngine;
        sol::state_view mStateView { mState };
        std::queue<fs::path> mPaths;
        std::recursive_mutex mPathsMutex;
        std::mt19937 mMersenneTwister;
        std::uniform_real_distribution<double> mUniformRealDistribution01;
        boost::scoped_thread<> mThread;
    };

    struct TimedEvent {
        std::chrono::high_resolution_clock::duration Duration {};
        std::chrono::high_resolution_clock::time_point LastCompletion {};
        std::string EventName;
        TLuaStateId StateId;
        CallStrategy Strategy;
        bool Expired();
        void Reset();
    };

    std::shared_ptr<::Network> mNetwork;
    const fs::path mResourceServerPath;
    std::vector<std::shared_ptr<TLuaPlugin>> mLuaPlugins;
    std::unordered_map<TLuaStateId, std::unique_ptr<StateThreadData>> mLuaStates;
    std::recursive_mutex mLuaStatesMutex;
    std::unordered_map<std::string /* event name */, std::unordered_map<TLuaStateId, std::set<std::string>>> mLuaEvents;
    std::recursive_mutex mLuaEventsMutex;
    std::vector<TimedEvent> mTimedEvents;
    std::recursive_mutex mTimedEventsMutex;
    std::list<std::shared_ptr<TLuaResult>> mResultsToCheck;
    std::mutex mResultsToCheckMutex;
    std::condition_variable mResultsToCheckCond;
    boost::scoped_thread<> mThread;
};

// std::any TriggerLuaEvent(const std::string& Event, bool local, TLuaPlugin* Caller, std::shared_ptr<TLuaArg> arg, bool Wait);
