// BeamMP, the BeamNG.drive multiplayer mod.
// Copyright (C) 2024 BeamMP Ltd., BeamMP team and contributors.
//
// BeamMP Ltd. can be contacted by electronic mail via contact@beammp.com.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include "Profiling.h"
#include "TNetwork.h"
#include "TServer.h"
#include <any>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <initializer_list>
#include <list>
#include <lua.hpp>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <queue>
#include <random>
#include <set>
#include <toml.hpp>
#include <unordered_map>
#include <vector>

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

struct JsonString {
    std::string value;
};

// value used to keep nils in a table or array, across serialization boundaries like
// JsonEncode, so that the nil stays at the same index and isn't treated like a special
// value (e.g. one that can be ignored or discarded).
const inline std::string BEAMMP_INTERNAL_NIL = "BEAMMP_SERVER_INTERNAL_NIL_VALUE";

using TLuaStateId = std::string;
namespace fs = std::filesystem;
/**
 * std::variant means, that TLuaArgTypes may be one of the Types listed as template args
 */
using TLuaValue = std::variant<std::string, int, JsonString, bool, std::unordered_map<std::string, std::string>, float>;
enum TLuaType {
    String = 0,
    Int = 1,
    Json = 2,
    Bool = 3,
    StringStringMap = 4,
    Float = 5,
};

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

class TLuaEngine : public std::enable_shared_from_this<TLuaEngine>, IThreaded {
public:
    enum CallStrategy : int {
        BestEffort,
        Precise,
    };

    struct QueuedFunction {
        std::string FunctionName;
        std::shared_ptr<TLuaResult> Result;
        std::vector<TLuaValue> Args;
        std::string EventName; // optional, may be empty
    };

    TLuaEngine();
    virtual ~TLuaEngine() noexcept {
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
    [[nodiscard]] std::shared_ptr<TLuaResult> EnqueueFunctionCall(TLuaStateId StateID, const std::string& FunctionName, const std::vector<TLuaValue>& Args);
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
        std::vector<TLuaValue> Arguments { TLuaValue { std::forward<ArgsT>(Args) }... };

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
        std::vector<TLuaValue> Arguments { TLuaValue { std::forward<ArgsT>(Args) }... };
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

    class StateThreadData : IThreaded {
    public:
        StateThreadData(const std::string& Name, TLuaStateId StateId, TLuaEngine& Engine);
        StateThreadData(const StateThreadData&) = delete;
        virtual ~StateThreadData() noexcept { beammp_debug("\"" + mStateId + "\" destroyed"); }
        [[nodiscard]] std::shared_ptr<TLuaResult> EnqueueScript(const TLuaChunk& Script);
        [[nodiscard]] std::shared_ptr<TLuaResult> EnqueueFunctionCall(const std::string& FunctionName, const std::vector<TLuaValue>& Args);
        [[nodiscard]] std::shared_ptr<TLuaResult> EnqueueFunctionCallFromCustomEvent(const std::string& FunctionName, const std::vector<TLuaValue>& Args, const std::string& EventName, CallStrategy Strategy);
        void RegisterEvent(const std::string& EventName, const std::string& FunctionName);
        void AddPath(const fs::path& Path); // to be added to path and cpath
        void operator()() override;
        sol::state_view State() { return sol::state_view(mState); }

        std::vector<std::string> GetStateGlobalKeys();
        std::vector<std::string> GetStateTableKeys(const std::vector<std::string>& keys);

        // Debug functions, slow
        std::queue<std::pair<TLuaChunk, std::shared_ptr<TLuaResult>>> Debug_GetStateExecuteQueue();
        std::vector<TLuaEngine::QueuedFunction> Debug_GetStateFunctionQueue();

    private:
        sol::table Lua_TriggerGlobalEvent(const std::string& EventName, sol::variadic_args EventArgs);
        sol::table Lua_TriggerLocalEvent(const std::string& EventName, sol::variadic_args EventArgs);
        sol::table Lua_GetPlayerIdentifiers(int ID);
        sol::table Lua_GetPlayers();
        std::string Lua_GetPlayerName(int ID);
        sol::table Lua_GetPlayerVehicles(int ID);
        std::pair<sol::table, std::string> Lua_GetPositionRaw(int PID, int VID);
        sol::table Lua_HttpCreateConnection(const std::string& host, uint16_t port);
        sol::table Lua_JsonDecode(const std::string& str);
        int Lua_GetPlayerIDByName(const std::string& Name);
        sol::table Lua_FS_ListFiles(const std::string& Path);
        sol::table Lua_FS_ListDirectories(const std::string& Path);

        prof::UnitProfileCollection mProfile {};
        std::unordered_map<std::string, prof::TimePoint> mProfileStarts;

        std::string mName;
        TLuaStateId mStateId;
        lua_State* mState;
        std::thread mThread;
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
        std::vector<sol::object> JsonStringToArray(JsonString Str);
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

    TNetwork* mNetwork;
    TServer* mServer;
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
};

// std::any TriggerLuaEvent(const std::string& Event, bool local, TLuaPlugin* Caller, std::shared_ptr<TLuaArg> arg, bool Wait);
