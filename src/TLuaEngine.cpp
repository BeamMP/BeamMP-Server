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

#include "TLuaEngine.h"
#include "Client.h"
#include "Common.h"
#include "CustomAssert.h"
#include "Env.h"
#include "Http.h"
#include "LuaAPI.h"
#include "Profiling.h"
#include "TLuaPlugin.h"
#include "TLuaResult.h"
#include "sol/object.hpp"

#include <chrono>
#include <condition_variable>
#include <fmt/core.h>
#include <mutex>
#include <nlohmann/json.hpp>
#include <random>
#include <sol/types.hpp>
#include <thread>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <variant>

TLuaEngine* LuaAPI::MP::Engine;

TLuaEngine::TLuaEngine()
    : mResourceServerPath(fs::path(Application::Settings.getAsString(Settings::Key::General_ResourceFolder)) / "Server") {
    Application::SetSubsystemStatus("LuaEngine", Application::Status::Starting);
    LuaAPI::MP::Engine = this;
    if (!fs::exists(Application::Settings.getAsString(Settings::Key::General_ResourceFolder))) {
        fs::create_directory(Application::Settings.getAsString(Settings::Key::General_ResourceFolder));
    }
    if (!fs::exists(mResourceServerPath)) {
        fs::create_directory(mResourceServerPath);
    }
    Application::RegisterShutdownHandler([&] {
        Application::SetSubsystemStatus("LuaEngine", Application::Status::ShuttingDown);
        if (mThread.joinable()) {
            mThread.join();
        }
        Application::SetSubsystemStatus("LuaEngine", Application::Status::Shutdown);
    });
    IThreaded::Start();
}

void TLuaEngine::operator()() {
    RegisterThread("LuaEngine");
    // lua engine main thread
    beammp_infof("Lua v{}.{}.{}", LUA_VERSION_MAJOR, LUA_VERSION_MINOR, LUA_VERSION_RELEASE);
    CollectAndInitPlugins();

    Application::SetSubsystemStatus("LuaEngine", Application::Status::Good);
    // now call all onInit's
    auto Futures = TriggerEvent("onInit", "");
    WaitForAll(Futures, std::chrono::seconds(5));
    for (const auto& Future : Futures) {
        auto Snapshot = Future->GetDetachedSnapshot();
        if (Snapshot.Error && Snapshot.ErrorMessage != BeamMPFnNotFoundError) {
            beammp_lua_error("Calling \"onInit\" on \"" + Snapshot.StateId + "\" failed: " + Snapshot.ErrorMessage);
        }
    }

    auto ResultCheckThread = std::thread([&] {
        RegisterThread("ResultCheckThread");
        while (!Application::IsShuttingDown()) {
            std::unique_lock Lock(mResultsToCheckMutex);
            if (!mResultsToCheck.empty()) {
                mResultsToCheck.remove_if([](const std::shared_ptr<TLuaResult>& Ptr) -> bool {
                    if (Ptr->IsReady()) {
                        auto Snapshot = Ptr->GetDetachedSnapshot();
                        if (Snapshot.Error) {
                            if (Snapshot.ErrorMessage != BeamMPFnNotFoundError) {
                                beammp_lua_error(Snapshot.Function + ": " + Snapshot.ErrorMessage);
                            }
                        }
                        return true;
                    }
                    return false;
                });
            } else {
                mResultsToCheckCond.wait_for(Lock, std::chrono::milliseconds(20));
            }
        }
    });
    // event loop
    auto Before = std::chrono::high_resolution_clock::now();
    while (!Application::IsShuttingDown()) {
        { // Timed Events Scope
            std::unique_lock Lock(mTimedEventsMutex);
            for (auto& Timer : mTimedEvents) {
                if (Timer.Expired()) {
                    auto LastCompletionBeforeReset = Timer.LastCompletion;
                    Timer.Reset();
                    auto Handlers = GetEventHandlersForState(Timer.EventName, Timer.StateId);
                    std::unique_lock StateLock(mLuaStatesMutex);
                    std::unique_lock Lock2(mResultsToCheckMutex);
                    for (auto& Handler : Handlers) {
                        auto Res = mLuaStates[Timer.StateId]->EnqueueFunctionCallFromCustomEvent(Handler, { }, Timer.EventName, Timer.Strategy);
                        if (Res) {
                            mResultsToCheck.push_back(Res);
                            mResultsToCheckCond.notify_one();
                        } else {
                            // "revert" reset
                            Timer.LastCompletion = LastCompletionBeforeReset;
                            // beammp_trace("Reverted reset of \"" + Timer.EventName + "\" timer");
                            // no need to try to enqueue more handlers for this event (they will all fail)
                            break;
                        }
                    }
                }
            }
        }
        bool StatesEmpty = false;
        {
            std::unique_lock Lock(mLuaStatesMutex);
            StatesEmpty = mLuaStates.empty();
        }

        if (StatesEmpty) {
            beammp_trace("No Lua states, event loop running extremely sparsely");
            Application::SleepSafeSeconds(10);
        } else {
            constexpr double NsFactor = 1000000.0;
            constexpr double Expected = 10.0; // ms
            const auto Diff = (std::chrono::high_resolution_clock::now() - Before).count() / NsFactor;
            if (Diff < Expected) {
                std::this_thread::sleep_for(std::chrono::nanoseconds(size_t((Expected - Diff) * NsFactor)));
            } else {
                beammp_tracef("Event loop cannot keep up! Running {}ms behind", Diff);
            }
        }
        Before = std::chrono::high_resolution_clock::now();
    }

    if (ResultCheckThread.joinable()) {
        ResultCheckThread.join();
    }
}

size_t TLuaEngine::CalculateMemoryUsage() {
    size_t Usage = 0;
    std::unique_lock Lock(mLuaStatesMutex);
    for (auto& State : mLuaStates) {
        Usage += State.second->State().memory_used();
    }
    return Usage;
}

sol::state_view TLuaEngine::GetStateForPlugin(const fs::path& PluginPath) {
    for (const auto& Plugin : mLuaPlugins) {
        if (fs::equivalent(Plugin->GetFolder(), PluginPath)) {
            std::unique_lock Lock(mLuaStatesMutex);
            return mLuaStates.at(Plugin->GetConfig().StateId)->State();
        }
    }
    beammp_assert_not_reachable();
    return mLuaStates.begin()->second->State();
}

TLuaStateId TLuaEngine::GetStateIDForPlugin(const fs::path& PluginPath) {
    for (const auto& Plugin : mLuaPlugins) {
        if (fs::equivalent(Plugin->GetFolder(), PluginPath)) {
            std::unique_lock Lock(mLuaStatesMutex);
            return Plugin->GetConfig().StateId;
        }
    }
    beammp_assert_not_reachable();
    return "";
}

void TLuaEngine::AddResultToCheck(const std::shared_ptr<TLuaResult>& Result) {
    std::unique_lock Lock(mResultsToCheckMutex);
    mResultsToCheck.push_back(Result);
    mResultsToCheckCond.notify_one();
}

std::unordered_map<std::string /* event name */, std::vector<std::string> /* handlers */> TLuaEngine::Debug_GetEventsForState(TLuaStateId StateId) {
    std::unordered_map<std::string, std::vector<std::string>> Result;
    std::unique_lock Lock(mLuaEventsMutex);
    for (const auto& EventNameToEventMap : mLuaEvents) {
        for (const auto& IdSetOfHandlersPair : EventNameToEventMap.second) {
            if (IdSetOfHandlersPair.first == StateId) {
                for (const auto& Handler : IdSetOfHandlersPair.second) {
                    Result[EventNameToEventMap.first].push_back(Handler);
                }
            }
        }
    }
    return Result;
}

std::queue<std::pair<TLuaChunk, std::shared_ptr<TLuaVoidResult>>> TLuaEngine::Debug_GetStateExecuteQueueForState(TLuaStateId StateId) {
    std::queue<std::pair<TLuaChunk, std::shared_ptr<TLuaVoidResult>>> Result;
    std::unique_lock Lock(mLuaStatesMutex);
    Result = mLuaStates.at(StateId)->Debug_GetStateExecuteQueue();
    return Result;
}

std::vector<TLuaEngine::QueuedFunction> TLuaEngine::Debug_GetStateFunctionQueueForState(TLuaStateId StateId) {
    std::vector<TLuaEngine::QueuedFunction> Result;
    std::unique_lock Lock(mLuaStatesMutex);
    Result = mLuaStates.at(StateId)->Debug_GetStateFunctionQueue();
    return Result;
}

std::vector<TLuaResult::DetachedSnapshot> TLuaEngine::Debug_GetResultsToCheckForState(TLuaStateId StateId) {
    std::unique_lock Lock(mResultsToCheckMutex);
    auto ResultsToCheckCopy = mResultsToCheck;
    Lock.unlock();
    std::vector<TLuaResult::DetachedSnapshot> Result;
    while (!ResultsToCheckCopy.empty()) {
        auto ResultToCheck = std::move(ResultsToCheckCopy.front());
        ResultsToCheckCopy.pop_front();
        if (ResultToCheck->OwnerState() == StateId) {
            Result.push_back(ResultToCheck->GetDetachedSnapshot());
        }
    }
    return Result;
}

std::vector<std::string> TLuaEngine::GetStateGlobalKeysForState(TLuaStateId StateId) {
    std::unique_lock Lock(mLuaStatesMutex);
    auto Result = mLuaStates.at(StateId)->GetStateGlobalKeys();
    return Result;
}

std::vector<std::string> TLuaEngine::StateThreadData::GetStateGlobalKeys() {
    auto globals = mStateView.globals();
    std::vector<std::string> Result;
    for (const auto& [key, value] : globals) {
        Result.push_back(key.as<std::string>());
    }
    return Result;
}

std::vector<std::string> TLuaEngine::GetStateTableKeysForState(TLuaStateId StateId, std::vector<std::string> keys) {
    std::unique_lock Lock(mLuaStatesMutex);
    auto Result = mLuaStates.at(StateId)->GetStateTableKeys(keys);
    return Result;
}

std::vector<std::string> TLuaEngine::StateThreadData::GetStateTableKeys(const std::vector<std::string>& keys) {
    auto globals = mStateView.globals();

    sol::table current = globals;
    std::vector<std::string> Result { };

    for (const auto& [key, value] : current) {
        std::string s = key.as<std::string>();
        if (value.get_type() == sol::type::function) {
            s += "(";
        }
        Result.push_back(s);
    }

    if (!keys.empty()) {
        Result.clear();
    }

    for (size_t i = 0; i < keys.size(); ++i) {
        auto obj = current.get<sol::object>(keys.at(i));
        if (obj.get_type() == sol::type::lua_nil) {
            // error
            break;
        } else if (i == keys.size() - 1) {
            if (obj.get_type() == sol::type::table) {
                for (const auto& [key, value] : obj.as<sol::table>()) {
                    std::string s = key.as<std::string>();
                    if (value.get_type() == sol::type::function) {
                        s += "(";
                    }
                    Result.push_back(s);
                }
            } else {
                Result = { obj.as<std::string>() };
            }
            break;
        }
        if (obj.get_type() == sol::type::table) {
            current = obj;
        } else {
            // error
            break;
        }
    }

    return Result;
}

/*

    _G.a.b.c.d.

*/

void TLuaEngine::WaitForAll(std::vector<std::shared_ptr<TLuaResult>>& Results, const std::optional<std::chrono::high_resolution_clock::duration>& Max) {
    for (const auto& Result : Results) {
        bool Cancelled = false;
        size_t ms = 0;
        std::set<std::string> WarnedResults;

        while (!Result->IsReady() && !Cancelled) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            ms += 10;
            if (Max.has_value() && std::chrono::milliseconds(ms) > Max.value()) {
                auto Snapshot = Result->GetDetachedSnapshot();
                beammp_trace("'" + Snapshot.Function + "' in '" + Snapshot.StateId + "' did not finish executing in time (took: " + std::to_string(ms) + "ms).");
                Cancelled = true;
            } else if (ms > 1000 * 60) {
                auto Snapshot = Result->GetDetachedSnapshot();
                auto ResultId = Snapshot.StateId + "_" + Snapshot.Function;
                if (WarnedResults.count(ResultId) == 0) {
                    WarnedResults.insert(ResultId);
                    beammp_lua_warn("'" + Snapshot.Function + "' in '" + Snapshot.StateId + "' is taking very long. The event it's handling is too important to discard the result of this handler, but may block this event and possibly the whole lua state.");
                }
            }
        }

        auto Snapshot = Result->GetDetachedSnapshot();
        if (Cancelled) {
            beammp_lua_warn("'" + Snapshot.Function + "' in '" + Snapshot.StateId + "' failed to execute in time and was not waited for. It may still finish executing at a later time.");
            LuaAPI::MP::Engine->ReportErrors({ Result });
        } else if (Snapshot.Error) {
            if (Snapshot.ErrorMessage != BeamMPFnNotFoundError) {
                beammp_lua_error(Snapshot.Function + ": " + Snapshot.ErrorMessage);
            }
        }
    }
}

// run this on the error checking thread
void TLuaEngine::ReportErrors(const std::vector<std::shared_ptr<TLuaResult>>& Results) {
    std::unique_lock Lock2(mResultsToCheckMutex);
    for (const auto& Result : Results) {
        mResultsToCheck.push_back(Result);
        mResultsToCheckCond.notify_one();
    }
}

bool TLuaEngine::HasState(TLuaStateId StateId) {
    std::unique_lock Lock(mLuaStatesMutex);
    return mLuaStates.find(StateId) != mLuaStates.end();
}

std::shared_ptr<TLuaVoidResult> TLuaEngine::EnqueueScript(TLuaStateId StateID, const TLuaChunk& Script) {
    std::unique_lock Lock(mLuaStatesMutex);
    return mLuaStates.at(StateID)->EnqueueScript(Script);
}

std::shared_ptr<TLuaResult> TLuaEngine::EnqueueFunctionCall(TLuaStateId StateID, const std::string& FunctionName, const std::vector<TLuaValue>& Args, const std::string& EventName) {
    std::unique_lock Lock(mLuaStatesMutex);
    return mLuaStates.at(StateID)->EnqueueFunctionCall(FunctionName, Args, EventName);
}

void TLuaEngine::CollectAndInitPlugins() {
    if (!fs::exists(mResourceServerPath)) {
        fs::create_directories(mResourceServerPath);
    }

    std::vector<fs::path> PluginsEntries;
    for (const auto& Entry : fs::directory_iterator(mResourceServerPath)) {
        if (Entry.is_directory()) {
            PluginsEntries.push_back(Entry);
        } else {
            beammp_error("\"" + Entry.path().string() + "\" is not a directory, skipping");
        }
    }

    std::sort(PluginsEntries.begin(), PluginsEntries.end(), [](const fs::path& first, const fs::path& second) {
        auto firstStr = first.string();
        auto secondStr = second.string();
        std::transform(firstStr.begin(), firstStr.end(), firstStr.begin(), ::tolower);
        std::transform(secondStr.begin(), secondStr.end(), secondStr.begin(), ::tolower);
        return firstStr < secondStr;
    });

    for (const auto& Dir : PluginsEntries) {
        auto Path = fs::relative(Dir);
        TLuaPluginConfig Config { Path.stem().string() };
        FindAndParseConfig(Path, Config);
        InitializePlugin(Path, Config);
    }
}

void TLuaEngine::InitializePlugin(const fs::path& Folder, const TLuaPluginConfig& Config) {
    beammp_assert(fs::exists(Folder));
    beammp_assert(fs::is_directory(Folder));
    std::unique_lock Lock(mLuaStatesMutex);
    EnsureStateExists(Config.StateId, Folder.stem().string(), true);
    mLuaStates[Config.StateId]->AddPath(Folder); // add to cpath + path
    Lock.unlock();
    auto Plugin = std::make_shared<TLuaPlugin>(*this, Config, Folder);
    mLuaPlugins.emplace_back(std::move(Plugin));
}

void TLuaEngine::FindAndParseConfig(const fs::path& Folder, TLuaPluginConfig& Config) {
    auto ConfigFile = Folder / TLuaPluginConfig::FileName;
    if (fs::exists(ConfigFile) && fs::is_regular_file(ConfigFile)) {
        try {
            auto Data = toml::parse(ConfigFile);
            if (Data.contains("LuaStateID")) {
                auto ID = toml::find<std::string>(Data, "LuaStateID");
                if (!ID.empty()) {
                    beammp_debug("Plugin \"" + Folder.string() + "\" specified it wants LuaStateID \"" + ID + "\"");
                    Config.StateId = ID;
                } else {
                    beammp_debug("LuaStateID empty, using plugin name");
                }
            }
        } catch (const std::exception& e) {
            beammp_error(Folder.string() + ": " + e.what());
        }
    }
}

void TLuaEngine::EnsureStateExists(TLuaStateId StateId, const std::string& Name, bool DontCallOnInit) {
    beammp_assert(!StateId.empty());
    std::unique_lock Lock(mLuaStatesMutex);
    if (mLuaStates.find(StateId) == mLuaStates.end()) {
        beammp_debug("Creating lua state for state id \"" + StateId + "\"");
        auto DataPtr = std::make_unique<StateThreadData>(Name, StateId, *this);
        mLuaStates[StateId] = std::move(DataPtr);
        RegisterEvent("onInit", StateId, "onInit");
        if (!DontCallOnInit) {
            auto Res = EnqueueFunctionCall(StateId, "onInit", { }, "onInit");
            Res->WaitUntilReady();
            auto Snapshot = Res->GetDetachedSnapshot();
            if (Snapshot.Error && Snapshot.ErrorMessage != TLuaEngine::BeamMPFnNotFoundError) {
                beammp_lua_error("Calling \"onInit\" on \"" + StateId + "\" failed: " + Snapshot.ErrorMessage);
            }
        }
    }
}

void TLuaEngine::RegisterEvent(const std::string& EventName, TLuaStateId StateId, const std::string& FunctionName) {
    std::unique_lock Lock(mLuaEventsMutex);
    mLuaEvents[EventName][StateId].insert(FunctionName);
}

std::set<std::string> TLuaEngine::GetEventHandlersForState(const std::string& EventName, TLuaStateId StateId) {
    std::unique_lock Lock(mLuaEventsMutex);
    return mLuaEvents[EventName][StateId];
}

std::vector<sol::object> TLuaEngine::StateThreadData::JsonStringToArray(JsonString Str) {
    auto LocalTable = Lua_JsonDecode(Str.value).as<std::vector<sol::object>>();
    for (auto& value : LocalTable) {
        if (value.is<std::string>() && value.as<std::string>() == BEAMMP_INTERNAL_NIL) {
            value = sol::object { };
        }
    }
    return LocalTable;
}

sol::table TLuaEngine::StateThreadData::Lua_TriggerGlobalEvent(const std::string& EventName, sol::variadic_args EventArgs) {
    auto Table = mStateView.create_table();
    int i = 1;
    for (auto Arg : EventArgs) {
        switch (Arg.get_type()) {
        case sol::type::none:
        case sol::type::userdata:
        case sol::type::lightuserdata:
        case sol::type::thread:
        case sol::type::function:
        case sol::type::poly:
            Table.set(i, BEAMMP_INTERNAL_NIL);
            beammp_warnf("Passed a value of type '{}' to TriggerGlobalEvent(\"{}\", ...). This type can not be serialized, and cannot be passed between states. It will arrive as <nil> in handlers.", sol::type_name(EventArgs.lua_state(), Arg.get_type()), EventName);
            break;
        case sol::type::lua_nil:
            Table.set(i, BEAMMP_INTERNAL_NIL);
            break;
        case sol::type::string:
        case sol::type::number:
        case sol::type::boolean:
        case sol::type::table:
            Table.set(i, Arg);
            break;
        }
        ++i;
    }
    JsonString Str { LuaAPI::MP::JsonEncode(Table) };
    beammp_debugf("json: {}", Str.value);
    auto Return = mEngine->TriggerEvent(EventName, mStateId, Str);
    auto MyHandlers = mEngine->GetEventHandlersForState(EventName, mStateId);

    sol::variadic_results LocalArgs = JsonStringToArray(Str);
    for (const auto& Handler : MyHandlers) {
        auto Fn = mStateView[Handler];
        if (Fn.valid()) {
            auto LuaResult = Fn(LocalArgs);
            auto Result = std::make_shared<TLuaResult>(mStateId, Handler);
            if (LuaResult.valid()) {
                try {
                    Result->MarkReadySuccess(LuaResult);
                } catch (const std::exception& e) {
                    Result->MarkReadyError(fmt::format("Call was successful, but result could not be serialized"));
                }
            } else {
                Result->MarkReadyError("Function result in TriggerGlobalEvent was invalid");
            }
            Return.push_back(Result);
        }
    }
    sol::state_view StateView(mState);
    sol::table AsyncEventReturn = StateView.create_table();
    AsyncEventReturn["ReturnValueImpl"] = Return;
    AsyncEventReturn.set_function("IsDone",
        [](const sol::table& Self) -> bool {
            auto Vector = Self.get<std::vector<std::shared_ptr<TLuaResult>>>("ReturnValueImpl");
            for (const auto& Value : Vector) {
                if (!Value->IsReady()) {
                    return false;
                }
            }
            return true;
        });
    AsyncEventReturn.set_function("GetResults",
        [](const sol::table& Self, sol::this_state State) -> sol::table {
            sol::state_view StateView(State);
            sol::table Result = StateView.create_table();
            auto Vector = Self.get<std::vector<std::shared_ptr<TLuaResult>>>("ReturnValueImpl");
            auto DetachedToLuaObject = [&StateView](const auto& SelfConvert, const TDetachedLuaValue& value) -> sol::object {
                return std::visit([&StateView, &SelfConvert](auto&& arg) -> sol::object {
                    using T = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_same_v<T, TDetachedLuaValue::Array>) {
                        sol::table Table = StateView.create_table(static_cast<int>(arg.size()), 0);
                        size_t i = 1;
                        for (const auto& Elem : arg) {
                            Table.set(i, SelfConvert(SelfConvert, Elem));
                            ++i;
                        }
                        return sol::make_object(StateView, Table);
                    } else if constexpr (std::is_same_v<T, TDetachedLuaValue::Object>) {
                        sol::table Table = StateView.create_table();
                        for (const auto& [Key, Elem] : arg) {
                            Table.set(Key, SelfConvert(SelfConvert, *Elem));
                        }
                        return sol::make_object(StateView, Table);
                    }
                    else if constexpr (std::is_same_v<T, bool>)
                        return sol::make_object(StateView, arg);
                    else if constexpr (std::is_same_v<T, double>)
                        return sol::make_object(StateView, arg);
                    else if constexpr (std::is_same_v<T, int>)
                        return sol::make_object(StateView, arg);
                    else if constexpr (std::is_same_v<T, std::string>)
                        return sol::make_object(StateView, arg);
                    else if constexpr (std::is_same_v<T, std::monostate>)
                        return sol::make_object(StateView, sol::lua_nil_t());
                    else
                        static_assert(AlwaysFalseV<T>, "non-exhaustive visitor!");
                }, value.V);
            };
            int i = 1;
            for (const auto& Value : Vector) {
                if (!Value->IsReady()) {
                    return sol::lua_nil;
                }
                auto Snapshot = Value->GetDetachedSnapshot();
                Result.set(i, DetachedToLuaObject(DetachedToLuaObject, Snapshot.Result));

                ++i;
            }
            return Result;
        });
    return AsyncEventReturn;
}

sol::table TLuaEngine::StateThreadData::Lua_TriggerLocalEvent(const std::string& EventName, sol::variadic_args EventArgs) {
    // TODO: make asynchronous?
    sol::table Result = mStateView.create_table();
    int i = 1;
    for (const auto& Handler : mEngine->GetEventHandlersForState(EventName, mStateId)) {
        auto Fn = mStateView[Handler];
        if (Fn.valid() && Fn.get_type() == sol::type::function) {
            auto FnRet = Fn(EventArgs);
            if (FnRet.valid()) {
                Result.set(i, FnRet);
                ++i;
            } else {
                std::string ErrStr;
                if (FnRet.get_type() == sol::type::string) {
                    ErrStr = FnRet.get<sol::error>().what();
                } else {
                    ErrStr = "(unknown error; error object is not inspectable)";
                }
                beammp_lua_errorf("TriggerLocalEvent: {}", ErrStr);
            }
        }
    }
    return Result;
}

sol::table TLuaEngine::StateThreadData::Lua_GetPlayerIdentifiers(int ID) {
    auto MaybeClient = GetClient(mEngine->Server(), ID);
    if (MaybeClient) {
        if (std::shared_ptr<TClient> Locked = MaybeClient.value().lock()) {
            auto IDs = Locked->GetIdentifiers();
            if (IDs.empty()) {
                return sol::lua_nil;
            }
            sol::table Result = mStateView.create_table();
            for (const auto& Pair : IDs) {
                Result.set(Pair.first, Pair.second);
            }
            return Result;
        }
    }
    return sol::lua_nil;
}

std::variant<std::string, sol::nil_t> TLuaEngine::StateThreadData::Lua_GetPlayerRole(int ID) {
    auto MaybeClient = GetClient(mEngine->Server(), ID);
    if (MaybeClient) {
        if (auto Locked = MaybeClient.value().lock()) {
            return Locked->GetRoles();
        }
    }
    return sol::nil;
}

sol::table TLuaEngine::StateThreadData::Lua_GetPlayers() {
    sol::table Result = mStateView.create_table();
    mEngine->Server().ForEachClient([&](std::weak_ptr<TClient> Client) -> bool {
        if (auto Locked = Client.lock()) {
            Result[Locked->GetID()] = Locked->GetName();
        }
        return true;
    });
    return Result;
}

int TLuaEngine::StateThreadData::Lua_GetPlayerIDByName(const std::string& Name) {
    int Id = -1;
    mEngine->mServer->ForEachClient([&Id, &Name](std::weak_ptr<TClient> Client) -> bool {
        if (auto Locked = Client.lock()) {
            if (Locked->GetName() == Name) {
                Id = Locked->GetID();
                return false;
            }
        }
        return true;
    });
    return Id;
}

sol::table TLuaEngine::StateThreadData::Lua_FS_ListFiles(const std::string& Path) {
    if (!std::filesystem::exists(Path)) {
        return sol::lua_nil;
    }
    auto table = mStateView.create_table();
    for (const auto& entry : std::filesystem::directory_iterator(Path)) {
        if (entry.is_regular_file() || entry.is_symlink()) {
            table[table.size() + 1] = entry.path().lexically_relative(Path).string();
        }
    }
    return table;
}

sol::table TLuaEngine::StateThreadData::Lua_FS_ListDirectories(const std::string& Path) {
    if (!std::filesystem::exists(Path)) {
        return sol::lua_nil;
    }
    auto table = mStateView.create_table();
    for (const auto& entry : std::filesystem::directory_iterator(Path)) {
        if (entry.is_directory()) {
            table[table.size() + 1] = entry.path().lexically_relative(Path).string();
        }
    }
    return table;
}

std::string TLuaEngine::StateThreadData::Lua_GetPlayerName(int ID) {
    auto MaybeClient = GetClient(mEngine->Server(), ID);
    if (MaybeClient) {
        if (auto Locked = MaybeClient.value().lock()) {
            return Locked->GetName();
        }
    }
    return "";
}

sol::table TLuaEngine::StateThreadData::Lua_GetPlayerVehicles(int ID) {
    auto MaybeClient = GetClient(mEngine->Server(), ID);
    if (MaybeClient) {
        if (auto Client = MaybeClient.value().lock()) {
            TClient::TSetOfVehicleData VehicleData;
            { // Vehicle Data Lock Scope
                auto LockedData = Client->GetAllCars();
                VehicleData = *LockedData.VehicleData;
            } // End Vehicle Data Lock Scope
            if (VehicleData.empty()) {
                return sol::lua_nil;
            }
            sol::state_view StateView(mState);
            sol::table Result = StateView.create_table();
            for (const auto& v : VehicleData) {
                Result[v.ID()] = v.DataAsPacket(Client->GetRoles(), Client->GetName(), Client->GetID()).substr(3);
            }
            return Result;
        }
    }
    return sol::lua_nil;
}

std::pair<sol::table, std::string> TLuaEngine::StateThreadData::Lua_GetPositionRaw(int PID, int VID) {
    std::pair<sol::table, std::string> Result;
    auto MaybeClient = GetClient(mEngine->Server(), PID);
    if (MaybeClient) {
        if (auto Client = MaybeClient.value().lock()) {
            std::string VehiclePos = Client->GetCarPositionRaw(VID);

            if (VehiclePos.empty()) {
                Result.second = "Vehicle not found";
                return Result;
            }

            sol::table t = Lua_JsonDecode(VehiclePos);
            if (t == sol::lua_nil) {
                Result.second = "Packet decode failed";
            }
            Result.first = t;
            return Result;
        }
    }
    Result.second = "No such player";
    return Result;
}

sol::table TLuaEngine::StateThreadData::Lua_HttpCreateConnection(const std::string& host, uint16_t port) {
    auto table = mStateView.create_table();
    constexpr const char* InternalClient = "__InternalClient";
    table["host"] = host;
    table["port"] = port;
    auto client = std::make_shared<httplib::Client>(host, port);
    table[InternalClient] = client;
    table.set_function("Get", [&InternalClient](const sol::table& table, const std::string& path, const sol::table& headers) {
        httplib::Headers GetHeaders;
        for (const auto& pair : headers) {
            if (pair.first.is<std::string>() && pair.second.is<std::string>()) {
                GetHeaders.insert(std::pair(pair.first.as<std::string>(), pair.second.as<std::string>()));
            } else {
                beammp_lua_error("Http:Get: Expected string-string pairs for headers, got something else, ignoring that header");
            }
        }
        auto client = table[InternalClient].get<std::shared_ptr<httplib::Client>>();
        client->Get(path.c_str(), GetHeaders);
    });
    return table;
}

template <typename T>
static void AddToTable(sol::table& table, const std::string& left, const T& value) {
    if (left.empty()) {
        table[table.size() + 1] = value;
    } else {
        table[left] = value;
    }
}

static bool mDisableMPSet = [] {
    auto DisableMPSet = Env::Get(Env::Key::PROVIDER_DISABLE_MP_SET).value_or("false");
    return DisableMPSet == "true" || DisableMPSet == "1";
}();

static auto GetSettingName = [](int id) -> const char* {
    switch (id) {
    case 0:
        return "Debug";
    case 1:
        return "Private";
    case 2:
        return "MaxCars";
    case 3:
        return "MaxPlayers";
    case 4:
        return "Map";
    case 5:
        return "Name";
    case 6:
        return "Description";
    case 7:
        return "InformationPacket";
    default:
        return "Unknown";
    }
};

static void JsonDecodeRecursive(sol::state_view& StateView, sol::table& table, const std::string& left, const nlohmann::json& right) {
    switch (right.type()) {
    case nlohmann::detail::value_t::null:
        AddToTable(table, left, sol::lua_nil_t { });
        return;
    case nlohmann::detail::value_t::object: {
        auto value = table.create();
        value.clear();
        for (const auto& entry : right.items()) {
            JsonDecodeRecursive(StateView, value, entry.key(), entry.value());
        }
        AddToTable(table, left, value);
        break;
    }
    case nlohmann::detail::value_t::array: {
        auto value = table.create();
        value.clear();
        for (const auto& entry : right.items()) {
            JsonDecodeRecursive(StateView, value, "", entry.value());
        }
        AddToTable(table, left, value);
        break;
    }
    case nlohmann::detail::value_t::string:
        AddToTable(table, left, right.get<std::string>());
        break;
    case nlohmann::detail::value_t::boolean:
        AddToTable(table, left, right.get<bool>());
        break;
    case nlohmann::detail::value_t::number_integer:
        AddToTable(table, left, right.get<int64_t>());
        break;
    case nlohmann::detail::value_t::number_unsigned:
        AddToTable(table, left, right.get<uint64_t>());
        break;
    case nlohmann::detail::value_t::number_float:
        AddToTable(table, left, right.get<double>());
        break;
    case nlohmann::detail::value_t::binary:
        beammp_lua_error("JsonDecode can't handle binary blob in json, ignoring");
        return;
    case nlohmann::detail::value_t::discarded:
        return;
    default:
        beammp_assert_not_reachable();
    }
}

sol::table TLuaEngine::StateThreadData::Lua_JsonDecode(const std::string& str) {
    sol::state_view StateView(mState);
    auto table = StateView.create_table();
    if (!nlohmann::json::accept(str)) {
        beammp_lua_error("string given to JsonDecode is not valid json: `" + str + "`");
        return sol::lua_nil;
    }
    nlohmann::json json = nlohmann::json::parse(str);
    if (json.is_object()) {
        for (const auto& entry : json.items()) {
            JsonDecodeRecursive(StateView, table, entry.key(), entry.value());
        }
    } else if (json.is_array()) {
        for (const auto& entry : json) {
            JsonDecodeRecursive(StateView, table, "", entry);
        }
    } else {
        beammp_lua_error("JsonDecode expected array or object json, instead got " + std::string(json.type_name()));
        return sol::lua_nil;
    }
    return table;
}

TLuaEngine::StateThreadData::StateThreadData(const std::string& Name, TLuaStateId StateId, TLuaEngine& Engine)
    : mName(Name)
    , mStateId(StateId)
    , mState(luaL_newstate())
    , mEngine(&Engine) {
    if (!mState) {
        beammp_error("failed to create lua state for \"" + StateId + "\"");
        return;
    }
    luaL_openlibs(mState);
    sol::state_view StateView(mState);
    lua_atpanic(mState, LuaAPI::PanicHandler);
    // StateView.globals()["package"].get()
    StateView.set_function("print", &LuaAPI::Print);
    StateView.set_function("printRaw", &LuaAPI::MP::PrintRaw);
    StateView.set_function("exit", &Application::GracefullyShutdown);

    auto MPTable = StateView.create_named_table("MP");
    MPTable.set_function("CreateTimer", [&]() -> sol::table {
        sol::state_view StateView(mState);
        sol::table Result = StateView.create_table();
        Result["__StartTime"] = std::chrono::high_resolution_clock::now();
        Result.set_function("GetCurrent", [&](const sol::table& Table) -> float {
            auto End = std::chrono::high_resolution_clock::now();
            auto Start = Table.get<std::chrono::high_resolution_clock::time_point>("__StartTime");
            return std::chrono::duration_cast<std::chrono::microseconds>(End - Start).count() / 1000000.0f;
        });
        Result.set_function("Start", [&](sol::table Table) {
            Table["__StartTime"] = std::chrono::high_resolution_clock::now();
        });
        return Result;
    });
    MPTable.set_function("GetOSName", &LuaAPI::MP::GetOSName);
    MPTable.set_function("GetServerVersion", &LuaAPI::MP::GetServerVersion);
    MPTable.set_function("RegisterEvent", [this](const std::string& EventName, const std::string& FunctionName) {
        RegisterEvent(EventName, FunctionName);
    });
    MPTable.set_function("TriggerGlobalEvent", [&](const std::string& EventName, sol::variadic_args EventArgs) -> sol::table {
        return Lua_TriggerGlobalEvent(EventName, EventArgs);
    });
    MPTable.set_function("TriggerLocalEvent", [&](const std::string& EventName, sol::variadic_args EventArgs) -> sol::table {
        return Lua_TriggerLocalEvent(EventName, EventArgs);
    });
    MPTable.set_function("TriggerClientEvent", &LuaAPI::MP::TriggerClientEvent);
    MPTable.set_function("TriggerClientEventJson", &LuaAPI::MP::TriggerClientEventJson);
    MPTable.set_function("TriggerClientEventUnreliable", &LuaAPI::MP::TriggerClientEventUnreliable);
    MPTable.set_function("TriggerClientEventJsonUnreliable", &LuaAPI::MP::TriggerClientEventJsonUnreliable);
    MPTable.set_function("GetPlayerCount", &LuaAPI::MP::GetPlayerCount);
    MPTable.set_function("IsPlayerConnected", &LuaAPI::MP::IsPlayerConnected);
    MPTable.set_function("GetPlayerIDByName", [&](const std::string& Name) -> int {
        return Lua_GetPlayerIDByName(Name);
    });
    MPTable.set_function("GetPlayerName", [&](int ID) -> std::string {
        return Lua_GetPlayerName(ID);
    });
    MPTable.set_function("RemoveVehicle", &LuaAPI::MP::RemoveVehicle);
    MPTable.set_function("GetPlayerVehicles", [&](int ID) -> sol::table {
        return Lua_GetPlayerVehicles(ID);
    });
    MPTable.set_function("GetPositionRaw", [&](int PID, int VID) -> std::pair<sol::table, std::string> {
        return Lua_GetPositionRaw(PID, VID);
    });
    MPTable.set_function("SendChatMessage", [&](sol::variadic_args Args) {
        if (Args.size() == 2) {
            LuaAPI::MP::SendChatMessage(Args.get<int>(0), Args.get<std::string>(1));
        } else if (Args.size() == 3) {
            LuaAPI::MP::SendChatMessage(Args.get<int>(0), Args.get<std::string>(1), Args.get<bool>(2));
        } else {
            beammp_lua_error("SendChatMessage expects 2 or 3 arguments.");
        }
    });
    MPTable.set_function("SendNotification", [&](sol::variadic_args Args) {
        if (Args.size() == 2) {
            LuaAPI::MP::SendNotification(Args.get<int>(0), Args.get<std::string>(1), "", Args.get<std::string>(1));
        } else if (Args.size() == 3) {
            LuaAPI::MP::SendNotification(Args.get<int>(0), Args.get<std::string>(1), Args.get<std::string>(2), Args.get<std::string>(1));
        } else if (Args.size() == 4) {
            LuaAPI::MP::SendNotification(Args.get<int>(0), Args.get<std::string>(1), Args.get<std::string>(2), Args.get<std::string>(3));
        } else {
            beammp_lua_error("SendNotification expects 2, 3 or 4 arguments.");
        }
    });
    MPTable.set_function("ConfirmationDialog", sol::overload(&LuaAPI::MP::ConfirmationDialog, [&](const int& ID, const std::string& Title, const std::string& Body, const sol::table& Buttons, const std::string& InteractionID) {
        LuaAPI::MP::ConfirmationDialog(ID, Title, Body, Buttons, InteractionID);
    }));
    MPTable.set_function("GetPlayers", [&]() -> sol::table {
        return Lua_GetPlayers();
    });
    MPTable.set_function("IsPlayerGuest", &LuaAPI::MP::IsPlayerGuest);
    MPTable.set_function("DropPlayer", &LuaAPI::MP::DropPlayer);
    MPTable.set_function("GetStateMemoryUsage", [&]() -> size_t {
        return mStateView.memory_used();
    });
    MPTable.set_function("GetLuaMemoryUsage", [&]() -> size_t {
        return mEngine->CalculateMemoryUsage();
    });
    MPTable.set_function("GetPlayerIdentifiers", [&](int ID) -> sol::table {
        return Lua_GetPlayerIdentifiers(ID);
    });
    MPTable.set_function("GetPlayerRole", [&](int ID) -> std::variant<std::string, sol::nil_t> {
        return Lua_GetPlayerRole(ID);
    });
    MPTable.set_function("Sleep", &LuaAPI::MP::Sleep);
    //  const std::string& EventName, size_t IntervalMS, int strategy
    MPTable.set_function("CreateEventTimer", [&](sol::variadic_args Args) {
        if (Args.size() < 2 || Args.size() > 3) {
            beammp_lua_error("CreateEventTimer expects 2 or 3 arguments.");
        }
        if (Args.get_type(0) != sol::type::string) {
            beammp_lua_error("CreateEventTimer expects 1st argument to be a string");
        }
        if (Args.get_type(1) != sol::type::number) {
            beammp_lua_error("CreateEventTimer expects 2nd argument to be a number");
        }
        if (Args.size() == 3 && Args.get_type(2) != sol::type::number) {
            beammp_lua_error("CreateEventTimer expects 3rd argument to be a number (MP.CallStrategy)");
        }
        auto EventName = Args.get<std::string>(0);
        auto IntervalMS = Args.get<size_t>(1);
        CallStrategy Strategy = Args.size() > 2 ? Args.get<CallStrategy>(2) : CallStrategy::BestEffort;
        if (IntervalMS < 25) {
            beammp_warn("Timer for \"" + EventName + "\" on \"" + mStateId + "\" is set to trigger at <25ms, which is likely too fast and won't cancel properly.");
        }
        mEngine->CreateEventTimer(EventName, mStateId, IntervalMS, Strategy);
    });
    MPTable.set_function("CancelEventTimer", [&](const std::string& EventName) {
        mEngine->CancelEventTimers(EventName, mStateId);
    });
    if (mDisableMPSet) {
        MPTable.set_function("Set", [this](int ConfigID, sol::object NewValue) {
            beammp_lua_errorf("A script ({}) tried to call MP.Set to change setting '{}' but this was blocked by your server provider.", mStateId, GetSettingName(ConfigID));
        });
    } else {
        MPTable.set_function("Set", &LuaAPI::MP::Set);
    }
    MPTable.set_function("Get", &LuaAPI::MP::Get);

    MPTable.set_function("GetServerTimeMS", [this](const sol::this_state ts) {
        return make_object(sol::state_view(ts), this->mEngine->Server().GetServerTimeMS());
    });

    MPTable.set_function("GetServerTime", [this](const sol::this_state ts) {
        return make_object(sol::state_view(ts), this->mEngine->Server().GetServerTime());
    });

    auto UtilTable = StateView.create_named_table("Util");
    UtilTable.set_function("LogDebug", [this](sol::variadic_args args) {
        std::string ToPrint = "";
        for (const auto& arg : args) {
            ToPrint += LuaAPI::LuaToString(static_cast<const sol::object>(arg));
            ToPrint += "\t";
        }
        if (Application::Settings.getAsBool(Settings::Key::General_Debug)) {
            beammp_lua_log("DEBUG", mStateId, ToPrint);
        }
    });
    UtilTable.set_function("LogInfo", [this](sol::variadic_args args) {
        std::string ToPrint = "";
        for (const auto& arg : args) {
            ToPrint += LuaAPI::LuaToString(static_cast<const sol::object>(arg));
            ToPrint += "\t";
        }
        beammp_lua_log("INFO", mStateId, ToPrint);
    });
    UtilTable.set_function("LogWarn", [this](sol::variadic_args args) {
        std::string ToPrint = "";
        for (const auto& arg : args) {
            ToPrint += LuaAPI::LuaToString(static_cast<const sol::object>(arg));
            ToPrint += "\t";
        }
        beammp_lua_log("WARN", mStateId, ToPrint);
    });
    UtilTable.set_function("LogError", [this](sol::variadic_args args) {
        std::string ToPrint = "";
        for (const auto& arg : args) {
            ToPrint += LuaAPI::LuaToString(static_cast<const sol::object>(arg));
            ToPrint += "\t";
        }
        beammp_lua_log("ERROR", mStateId, ToPrint);
    });
    UtilTable.set_function("JsonEncode", &LuaAPI::MP::JsonEncode);
    UtilTable.set_function("JsonDecode", [this](const std::string& str) {
        return Lua_JsonDecode(str);
    });
    UtilTable.set_function("JsonDiff", &LuaAPI::MP::JsonDiff);
    UtilTable.set_function("JsonFlatten", &LuaAPI::MP::JsonFlatten);
    UtilTable.set_function("JsonUnflatten", &LuaAPI::MP::JsonUnflatten);
    UtilTable.set_function("JsonPrettify", &LuaAPI::MP::JsonPrettify);
    UtilTable.set_function("JsonMinify", &LuaAPI::MP::JsonMinify);
    UtilTable.set_function("Random", [this] {
        return mUniformRealDistribution01(mMersenneTwister);
    });
    UtilTable.set_function("RandomRange", [this](double min, double max) -> double {
        return std::uniform_real_distribution(min, max)(mMersenneTwister);
    });
    UtilTable.set_function("RandomIntRange", [this](int64_t min, int64_t max) -> int64_t {
        return std::uniform_int_distribution(min, max)(mMersenneTwister);
    });
    UtilTable.set_function("DebugExecutionTime", [this]() -> sol::table {
        sol::state_view StateView(mState);
        sol::table Result = StateView.create_table();
        auto stats = mProfile.all_stats();
        for (const auto& [name, stat] : stats) {
            Result[name] = StateView.create_table();
            Result[name]["mean"] = stat.mean;
            Result[name]["stdev"] = stat.stdev;
            Result[name]["min"] = stat.min;
            Result[name]["max"] = stat.max;
            Result[name]["n"] = stat.n;
        }
        return Result;
    });
    UtilTable.set_function("DebugStartProfile", [this](const std::string& name) {
        mProfileStarts[name] = prof::now();
    });
    UtilTable.set_function("DebugStopProfile", [this](const std::string& name) {
        if (!mProfileStarts.contains(name)) {
            beammp_lua_errorf("DebugStopProfile('{}') failed, because a profile for '{}' wasn't started", name, name);
            return;
        }
        mProfile.add_sample(name, prof::duration(mProfileStarts.at(name), prof::now()));
    });

    auto HttpTable = StateView.create_named_table("Http");
    HttpTable.set_function("CreateConnection", [this](const std::string& host, uint16_t port) {
        return Lua_HttpCreateConnection(host, port);
    });

    MPTable.create_named("Settings",
        "Debug", 0,
        "Private", 1,
        "MaxCars", 2,
        "MaxPlayers", 3,
        "Map", 4,
        "Name", 5,
        "Description", 6,
        "InformationPacket", 7);

    MPTable.create_named("CallStrategy",
        "BestEffort", CallStrategy::BestEffort,
        "Precise", CallStrategy::Precise);

    auto FSTable = StateView.create_named_table("FS");
    FSTable.set_function("CreateDirectory", &LuaAPI::FS::CreateDirectory);
    FSTable.set_function("Exists", &LuaAPI::FS::Exists);
    FSTable.set_function("Remove", &LuaAPI::FS::Remove);
    FSTable.set_function("Rename", &LuaAPI::FS::Rename);
    FSTable.set_function("Copy", &LuaAPI::FS::Copy);
    FSTable.set_function("GetFilename", &LuaAPI::FS::GetFilename);
    FSTable.set_function("GetExtension", &LuaAPI::FS::GetExtension);
    FSTable.set_function("GetParentFolder", &LuaAPI::FS::GetParentFolder);
    FSTable.set_function("IsDirectory", &LuaAPI::FS::IsDirectory);
    FSTable.set_function("IsFile", &LuaAPI::FS::IsFile);
    FSTable.set_function("ConcatPaths", &LuaAPI::FS::ConcatPaths);
    FSTable.set_function("ListFiles", [this](const std::string& Path) {
        return Lua_FS_ListFiles(Path);
    });
    FSTable.set_function("ListDirectories", [this](const std::string& Path) {
        return Lua_FS_ListDirectories(Path);
    });
    Start();
}

std::shared_ptr<TLuaVoidResult> TLuaEngine::StateThreadData::EnqueueScript(const TLuaChunk& Script) {
    std::unique_lock Lock(mStateExecuteQueueMutex);
    auto Result = std::make_shared<TLuaVoidResult>(mStateId);
    mStateExecuteQueue.push({ Script, Result });
    return Result;
}

std::shared_ptr<TLuaResult> TLuaEngine::StateThreadData::EnqueueFunctionCallFromCustomEvent(const std::string& FunctionName, const std::vector<TLuaValue>& Args, const std::string& EventName, CallStrategy Strategy) {
    // TODO: Document all this
    std::unique_lock Lock(mStateFunctionQueueMutex);
    decltype(mStateFunctionQueue)::iterator Iter = mStateFunctionQueue.end();
    if (Strategy == CallStrategy::BestEffort) {
        Iter = std::find_if(mStateFunctionQueue.begin(), mStateFunctionQueue.end(),
            [&EventName](const QueuedFunction& Element) {
                return Element.EventName == EventName;
            });
    }
    if (Iter == mStateFunctionQueue.end()) {
        auto Result = std::make_shared<TLuaResult>(mStateId, FunctionName);
        mStateFunctionQueue.push_back({ FunctionName, Result, Args, EventName });
        mStateFunctionQueueCond.notify_all();
        return Result;
    } else {
        return nullptr;
    }
}

std::shared_ptr<TLuaResult> TLuaEngine::StateThreadData::EnqueueFunctionCall(const std::string& FunctionName, const std::vector<TLuaValue>& Args, const std::string& EventName) {
    auto Result = std::make_shared<TLuaResult>(mStateId, FunctionName);
    std::unique_lock Lock(mStateFunctionQueueMutex);
    mStateFunctionQueue.push_back({ FunctionName, Result, Args, EventName });
    mStateFunctionQueueCond.notify_all();
    return Result;
}

void TLuaEngine::StateThreadData::RegisterEvent(const std::string& EventName, const std::string& FunctionName) {
    mEngine->RegisterEvent(EventName, mStateId, FunctionName);
}

void TLuaEngine::StateThreadData::operator()() {
    RegisterThread("Lua:" + mStateId);
    while (!Application::IsShuttingDown()) {
        { // StateExecuteQueue Scope
            std::unique_lock Lock(mStateExecuteQueueMutex);
            if (!mStateExecuteQueue.empty()) {
                auto S = mStateExecuteQueue.front();
                mStateExecuteQueue.pop();
                Lock.unlock();

                { // Paths Scope
                    std::unique_lock Lock(mPathsMutex);
                    if (!mPaths.empty()) {
                        std::stringstream PathAdditions;
                        std::stringstream CPathAdditions;
                        while (!mPaths.empty()) {
                            auto Path = mPaths.front();
                            mPaths.pop();
                            PathAdditions << ";" << (Path / "?.lua").string();
                            PathAdditions << ";" << (Path / "lua/?.lua").string();
#if WIN32
                            CPathAdditions << ";" << (Path / "?.dll").string();
                            CPathAdditions << ";" << (Path / "lib/?.dll").string();
#else // unix
                            CPathAdditions << ";" << (Path / "?.so").string();
                            CPathAdditions << ";" << (Path / "lib/?.so").string();
#endif
                        }
                        sol::state_view StateView(mState);
                        auto PackageTable = StateView.globals().get<sol::table>("package");
                        PackageTable["path"] = PackageTable.get<std::string>("path") + PathAdditions.str();
                        PackageTable["cpath"] = PackageTable.get<std::string>("cpath") + CPathAdditions.str();
                        StateView.globals()["package"] = PackageTable;
                    }
                }
                sol::state_view StateView(mState);
                auto Res = StateView.safe_script(*S.first.Content, sol::script_pass_on_error, S.first.FileName);
                if (Res.valid()) {
                    // Script-load completion should not serialize the script's return value.
                    // A loaded chunk may legally return non-serializable Lua values such as
                    // functions or function tables. For this reason, we don't pass anything
                    // to the result here.
                    S.second->MarkReadySuccess();
                } else {
                    S.second->MarkReadyError(std::move(Res));
                }
            }
        }
        { // StateFunctionQueue Scope
            std::unique_lock Lock(mStateFunctionQueueMutex);
            auto NotExpired = mStateFunctionQueueCond.wait_for(Lock,
                std::chrono::milliseconds(500),
                [&]() -> bool { return !mStateFunctionQueue.empty(); });
            if (NotExpired) {
                auto ProfStart = prof::now();
                auto TheQueuedFunction = std::move(mStateFunctionQueue.front());
                mStateFunctionQueue.erase(mStateFunctionQueue.begin());
                Lock.unlock();
                auto& FnName = TheQueuedFunction.FunctionName;
                auto& Result = TheQueuedFunction.Result;
                auto Args = TheQueuedFunction.Args;
                // TODO: Use TheQueuedFunction.EventName for errors, warnings, etc
                Result->SetOwnerState(mStateId);
                sol::state_view StateView(mState);
                auto Fn = StateView[FnName];
                if (Fn.valid() && Fn.get_type() == sol::type::function) {
                    std::vector<sol::object> LuaArgs;
                    for (const auto& Arg : Args) {
                        if (Arg.valueless_by_exception()) {
                            continue;
                        }
                        std::visit([&LuaArgs, &StateView, this](const auto& arg) {
                            using T = std::decay_t<decltype(arg)>;
                            if constexpr (std::is_same_v<T, std::string>) {
                                LuaArgs.push_back(sol::make_object(StateView, arg));
                            } else if constexpr (std::is_same_v<T, int>) {
                                LuaArgs.push_back(sol::make_object(StateView, arg));
                            } else if constexpr (std::is_same_v<T, bool>) {
                                LuaArgs.push_back(sol::make_object(StateView, arg));
                            } else if constexpr (std::is_same_v<T, JsonString>) {
                                auto LocalArgs = JsonStringToArray(arg);
                                LuaArgs.insert(LuaArgs.end(), LocalArgs.begin(), LocalArgs.end());
                            } else if constexpr (std::is_same_v<T, std::monostate>) {
                                beammp_lua_error("Unknown argument type, passed as nil");
                                LuaArgs.push_back(sol::lua_nil_t());
                            } else if constexpr (std::is_same_v<T, std::unordered_map<std::string, std::string>>) {
                                auto Table = StateView.create_table();
                                for (const auto& [k, v] : arg) {
                                    Table[k] = v;
                                }
                                LuaArgs.push_back(Table);
                            } else if constexpr (std::is_same_v<T, float>) {
                                LuaArgs.push_back(sol::make_object(StateView, arg));
                            } else {
                                static_assert(AlwaysFalseV<T>, "unhandled variant");
                            }
                        }, Arg);
                    }
                    auto Res = Fn(sol::as_args(LuaArgs));
                    if (Res.valid()) {
                        try {
                            Result->MarkReadySuccess(std::move(Res));
                        } catch (const std::exception& e) {
                            Result->MarkReadyError(fmt::format("Call was successful, but result could not be serialized"));
                        }
                    } else {
                        Result->MarkReadyError(std::move(Res));
                    }
                } else {
                    Result->MarkReadyError(BeamMPFnNotFoundError);
                }
                auto ProfEnd = prof::now();
                auto ProfDuration = prof::duration(ProfStart, ProfEnd);
                mProfile.add_sample(FnName, ProfDuration);
            }
        }
    }
}

std::queue<std::pair<TLuaChunk, std::shared_ptr<TLuaVoidResult>>> TLuaEngine::StateThreadData::Debug_GetStateExecuteQueue() {
    std::unique_lock Lock(mStateExecuteQueueMutex);
    return mStateExecuteQueue;
}

std::vector<TLuaEngine::QueuedFunction> TLuaEngine::StateThreadData::Debug_GetStateFunctionQueue() {
    std::unique_lock Lock(mStateFunctionQueueMutex);
    return mStateFunctionQueue;
}

void TLuaEngine::CreateEventTimer(const std::string& EventName, TLuaStateId StateId, size_t IntervalMS, CallStrategy Strategy) {
    std::unique_lock Lock(mTimedEventsMutex);
    TimedEvent Event {
        std::chrono::high_resolution_clock::duration { std::chrono::milliseconds(IntervalMS) },
        std::chrono::high_resolution_clock::now(),
        EventName,
        StateId,
        Strategy
    };
    mTimedEvents.push_back(std::move(Event));
    beammp_trace("created event timer for \"" + EventName + "\" on \"" + StateId + "\" with " + std::to_string(IntervalMS) + "ms interval");
}

void TLuaEngine::CancelEventTimers(const std::string& EventName, TLuaStateId StateId) {
    std::unique_lock Lock(mTimedEventsMutex);
    beammp_trace("cancelling event timer for \"" + EventName + "\" on \"" + StateId + "\"");
    for (;;) {
        auto Iter = std::find_if(mTimedEvents.begin(), mTimedEvents.end(), [&](const TimedEvent& Event) -> bool {
            return Event.EventName == EventName && Event.StateId == StateId;
        });
        if (Iter != mTimedEvents.end()) {
            mTimedEvents.erase(Iter);
        } else {
            break;
        }
    }
}

void TLuaEngine::StateThreadData::AddPath(const fs::path& Path) {
    std::unique_lock Lock(mPathsMutex);
    mPaths.push(Path);
}

TLuaChunk::TLuaChunk(std::shared_ptr<std::string> Content, std::string FileName, std::string PluginPath)
    : Content(Content)
    , FileName(FileName)
    , PluginPath(PluginPath) {
}

bool TLuaEngine::TimedEvent::Expired() {
    auto Waited = (std::chrono::high_resolution_clock::now() - LastCompletion);
    return Waited >= Duration;
}

void TLuaEngine::TimedEvent::Reset() {
    LastCompletion = std::chrono::high_resolution_clock::now();
}

TEST_CASE("TLuaEngine ctor & dtor") {
    Application::Settings.set(Settings::Key::General_ResourceFolder, "beammp_server_test_resources");
    TLuaEngine engine;

    const TLuaStateId StateId = "lua_event_contract_test";
    engine.EnsureStateExists(StateId, "LuaEventContractTest", true);

    // LLM generated test code
    auto Script = std::make_shared<std::string>(R"(
function onPlayerAuth(playerName, playerRole, isGuest, identifiers)
    if type(playerName) ~= "string" then return "on:bad-playerName-type:" .. type(playerName) end
    if type(playerRole) ~= "string" then return "on:bad-playerRole-type:" .. type(playerRole) end
    if type(isGuest) ~= "boolean" then return "on:bad-isGuest-type:" .. type(isGuest) end
    if type(identifiers) ~= "table" then return "on:bad-identifiers-type:" .. type(identifiers) end
    return "on:" .. playerName .. ":" .. playerRole .. ":" .. tostring(isGuest) .. ":" .. tostring(identifiers.ip) .. ":" .. tostring(identifiers.beammp)
end

function postPlayerAuth(isDenied, reason, playerName, playerRole, isGuest, identifiers)
    if type(isDenied) ~= "boolean" then return "post:bad-isDenied-type:" .. type(isDenied) end
    if type(reason) ~= "string" then return "post:bad-reason-type:" .. type(reason) end
    if type(playerName) ~= "string" then return "post:bad-playerName-type:" .. type(playerName) end
    if type(playerRole) ~= "string" then return "post:bad-playerRole-type:" .. type(playerRole) end
    if type(isGuest) ~= "boolean" then return "post:bad-isGuest-type:" .. type(isGuest) end
    if type(identifiers) ~= "table" then return "post:bad-identifiers-type:" .. type(identifiers) end
    return "post:" .. tostring(isDenied) .. ":" .. reason .. ":" .. playerName .. ":" .. playerRole .. ":" .. tostring(isGuest) .. ":" .. tostring(identifiers.ip) .. ":" .. tostring(identifiers.beammp)
end

function arrayBoundaryHandler()
    return { "first", "second", [4] = true }
end

function verifyArrayBoundaryRoundtrip()
    local pending = MP.TriggerGlobalEvent("arrayBoundaryEvent")
    if not pending:IsDone() then
        return "not_done"
    end

    local results = pending:GetResults()
    if type(results) ~= "table" then
        return "bad_results_type:" .. type(results)
    end
    if type(results[1]) ~= "table" then
        return "bad_item_type:" .. type(results[1])
    end

    local arr = results[1]
    return tostring(arr[1]) .. "|" .. tostring(arr[2]) .. "|" .. tostring(arr[4]) .. "|" .. tostring(arr[3] == nil)
end

MP.RegisterEvent("onPlayerAuth", "onPlayerAuth")
MP.RegisterEvent("postPlayerAuth", "postPlayerAuth")
MP.RegisterEvent("arrayBoundaryEvent", "arrayBoundaryHandler")
)");

    auto LoadResult = engine.EnqueueScript(StateId, TLuaChunk(Script, "event_contract.lua", "beammp_server_test_resources/Server/LuaEventContractTest"));
    LoadResult->WaitUntilReady();
    auto LoadSnapshot = LoadResult->GetSnapshot();
    CHECK(!LoadSnapshot.Error);

    const std::unordered_map<std::string, std::string> Identifiers {
        {"ip", "410.0.24.1"},
        {"beammp", "123456"},
    };

    auto OnPlayerAuthResults = engine.TriggerEvent(
        "onPlayerAuth", "",
        std::string("guest8133569"),
        std::string("USER"),
        true,
        Identifiers);
    REQUIRE(OnPlayerAuthResults.size() == 1);
    TLuaEngine::WaitForAll(OnPlayerAuthResults);

    auto OnPlayerAuthSnapshot = OnPlayerAuthResults.front()->GetDetachedSnapshot();
    CHECK(!OnPlayerAuthSnapshot.Error);
    const auto* OnPlayerAuthValue = std::get_if<std::string>(&OnPlayerAuthSnapshot.Result.V);
    REQUIRE(OnPlayerAuthValue != nullptr);
    CHECK(*OnPlayerAuthValue == "on:guest8133569:USER:true:410.0.24.1:123456");

    auto PostPlayerAuthResults = engine.TriggerEvent(
        "postPlayerAuth", "",
        false,
        std::string(""),
        std::string("guest8133569"),
        std::string("USER"),
        true,
        Identifiers);
    REQUIRE(PostPlayerAuthResults.size() == 1);
    TLuaEngine::WaitForAll(PostPlayerAuthResults);

    auto PostPlayerAuthSnapshot = PostPlayerAuthResults.front()->GetDetachedSnapshot();
    CHECK(!PostPlayerAuthSnapshot.Error);
    const auto* PostPlayerAuthValue = std::get_if<std::string>(&PostPlayerAuthSnapshot.Result.V);
    REQUIRE(PostPlayerAuthValue != nullptr);
    CHECK(*PostPlayerAuthValue == "post:false::guest8133569:USER:true:410.0.24.1:123456");

    auto ArrayRoundtrip = engine.EnqueueFunctionCall(StateId, "verifyArrayBoundaryRoundtrip", {}, "verifyArrayBoundaryRoundtrip");
    ArrayRoundtrip->WaitUntilReady();
    auto ArrayRoundtripSnapshot = ArrayRoundtrip->GetDetachedSnapshot();
    CHECK(!ArrayRoundtripSnapshot.Error);
    const auto* ArrayRoundtripValue = std::get_if<std::string>(&ArrayRoundtripSnapshot.Result.V);
    REQUIRE(ArrayRoundtripValue != nullptr);
    CHECK(*ArrayRoundtripValue == "first|second|true|true");

    Application::GracefullyShutdown();
}
