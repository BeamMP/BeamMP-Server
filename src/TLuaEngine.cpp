#include "TLuaEngine.h"
#include "Client.h"
#include "CustomAssert.h"
#include "Http.h"
#include "LuaAPI.h"
#include "TLuaPlugin.h"

#include <chrono>
#include <condition_variable>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <random>
#include <thread>
#include <tuple>

TLuaEngine* LuaAPI::MP::Engine;

TLuaEngine::TLuaEngine()
    : mPluginMonitor(fs::path(Application::Settings.Resource) / "Server", *this, mShutdown) {
    Application::SetSubsystemStatus("LuaEngine", Application::Status::Starting);
    LuaAPI::MP::Engine = this;
    if (!fs::exists(Application::Settings.Resource)) {
        fs::create_directory(Application::Settings.Resource);
    }
    fs::path Path = fs::path(Application::Settings.Resource) / "Server";
    if (!fs::exists(Path)) {
        fs::create_directory(Path);
    }
    mResourceServerPath = Path;
    Application::RegisterShutdownHandler([&] {
        Application::SetSubsystemStatus("LuaEngine", Application::Status::ShuttingDown);
        mShutdown = true;
        if (mThread.joinable()) {
            mThread.join();
        }
        Application::SetSubsystemStatus("LuaEngine", Application::Status::Shutdown);
    });
    Start();
}

void TLuaEngine::operator()() {
    RegisterThread("LuaEngine");
    Application::SetSubsystemStatus("LuaEngine", Application::Status::Good);
    // lua engine main thread
    CollectAndInitPlugins();
    // now call all onInit's
    auto Futures = TriggerEvent("onInit", "");
    WaitForAll(Futures);
    for (const auto& Future : Futures) {
        if (Future->Error && Future->ErrorMessage != BeamMPFnNotFoundError) {
            beammp_lua_error("Calling \"onInit\" on \"" + Future->StateId + "\" failed: " + Future->ErrorMessage);
        }
    }

    auto ResultCheckThread = std::thread([&] {
        RegisterThread("ResultCheckThread");
        while (!mShutdown) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            std::unique_lock Lock(mResultsToCheckMutex);
            if (!mResultsToCheck.empty()) {
                auto Res = mResultsToCheck.front();
                mResultsToCheck.pop();
                Lock.unlock();

                if (!Res->Ready) {
                    Lock.lock();
                    mResultsToCheck.push(Res);
                    Lock.unlock();
                }
                if (Res->Error) {
                    if (Res->ErrorMessage != BeamMPFnNotFoundError) {
                        beammp_lua_error(Res->Function + ": " + Res->ErrorMessage);
                    }
                }
            }
            std::this_thread::yield();
        }
    });
    // event loop
    auto Before = std::chrono::high_resolution_clock::now();
    while (!mShutdown) {
        if (mLuaStates.size() == 0) {
            std::this_thread::sleep_for(std::chrono::seconds(100));
        }
        { // Timed Events Scope
            std::unique_lock Lock(mTimedEventsMutex);
            for (auto& Timer : mTimedEvents) {
                if (Timer.Expired()) {
                    Timer.Reset();
                    auto Handlers = GetEventHandlersForState(Timer.EventName, Timer.StateId);
                    std::unique_lock StateLock(mLuaStatesMutex);
                    std::unique_lock Lock2(mResultsToCheckMutex);
                    for (auto& Handler : Handlers) {
                        auto Res = mLuaStates[Timer.StateId]->EnqueueFunctionCall(Handler, {});
                        mResultsToCheck.push(Res);
                    }
                }
            }
        }
        std::chrono::high_resolution_clock::duration Diff;
        if ((Diff = std::chrono::high_resolution_clock::now() - Before)
            < std::chrono::milliseconds(10)) {
            std::this_thread::sleep_for(Diff);
        } else {
            beammp_trace("Event loop cannot keep up! Running " + std::to_string(Diff.count()) + "s behind");
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
    mResultsToCheck.push(Result);
}

std::unordered_map<std::string /*event name */, std::vector<std::string> /* handlers */> TLuaEngine::Debug_GetEventsForState(TLuaStateId StateId) {
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

std::queue<std::pair<TLuaChunk, std::shared_ptr<TLuaResult>>> TLuaEngine::Debug_GetStateExecuteQueueForState(TLuaStateId StateId) {
    std::queue<std::pair<TLuaChunk, std::shared_ptr<TLuaResult>>> Result;
    std::unique_lock Lock(mLuaStatesMutex);
    Result = mLuaStates.at(StateId)->Debug_GetStateExecuteQueue();
    return Result;
}

std::queue<std::tuple<std::string, std::shared_ptr<TLuaResult>, std::vector<TLuaArgTypes>>> TLuaEngine::Debug_GetStateFunctionQueueForState(TLuaStateId StateId) {
    std::queue<std::tuple<std::string, std::shared_ptr<TLuaResult>, std::vector<TLuaArgTypes>>> Result;
    std::unique_lock Lock(mLuaStatesMutex);
    Result = mLuaStates.at(StateId)->Debug_GetStateFunctionQueue();
    return Result;
}

std::vector<TLuaResult> TLuaEngine::Debug_GetResultsToCheckForState(TLuaStateId StateId) {
    std::unique_lock Lock(mResultsToCheckMutex);
    auto ResultsToCheckCopy = mResultsToCheck;
    Lock.unlock();
    std::vector<TLuaResult> Result;
    while (!ResultsToCheckCopy.empty()) {
        auto ResultToCheck = std::move(ResultsToCheckCopy.front());
        ResultsToCheckCopy.pop();
        if (ResultToCheck->StateId == StateId) {
            Result.push_back(*ResultToCheck);
        }
    }
    return Result;
}

void TLuaEngine::WaitForAll(std::vector<std::shared_ptr<TLuaResult>>& Results, const std::optional<std::chrono::high_resolution_clock::duration>& Max) {
    for (const auto& Result : Results) {
        bool Cancelled = false;
        size_t ms = 0;
        while (!Result->Ready && !Cancelled) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            ms += 10;
            if (Max.has_value() && std::chrono::milliseconds(ms) > Max.value()) {
                beammp_trace("'" + Result->Function + "' in '" + Result->StateId + "' did not finish executing in time (took: " + std::to_string(ms) + "ms)");
                Cancelled = true;
            }
        }
        if (Cancelled) {
            beammp_lua_warn("'" + Result->Function + "' in '" + Result->StateId + "' failed to execute in time and was not waited for. It may still finish executing at a later time.");
            LuaAPI::MP::Engine->ReportErrors({ Result });
        } else if (Result->Error) {
            if (Result->ErrorMessage != BeamMPFnNotFoundError) {
                beammp_lua_error(Result->Function + ": " + Result->ErrorMessage);
            }
        }
    }
}

// run this on the error checking thread
void TLuaEngine::ReportErrors(const std::vector<std::shared_ptr<TLuaResult>>& Results) {
    std::unique_lock Lock2(mResultsToCheckMutex);
    for (const auto& Result : Results) {
        mResultsToCheck.push(Result);
    }
}

bool TLuaEngine::HasState(TLuaStateId StateId) {
    std::unique_lock Lock(mLuaStatesMutex);
    return mLuaStates.find(StateId) != mLuaStates.end();
}

std::shared_ptr<TLuaResult> TLuaEngine::EnqueueScript(TLuaStateId StateID, const TLuaChunk& Script) {
    std::unique_lock Lock(mLuaStatesMutex);
    return mLuaStates.at(StateID)->EnqueueScript(Script);
}

std::shared_ptr<TLuaResult> TLuaEngine::EnqueueFunctionCall(TLuaStateId StateID, const std::string& FunctionName, const std::vector<TLuaArgTypes>& Args) {
    std::unique_lock Lock(mLuaStatesMutex);
    return mLuaStates.at(StateID)->EnqueueFunctionCall(FunctionName, Args);
}

void TLuaEngine::CollectAndInitPlugins() {
    for (const auto& Dir : fs::directory_iterator(mResourceServerPath)) {
        auto Path = Dir.path();
        Path = fs::relative(Path);
        if (!Dir.is_directory()) {
            beammp_error("\"" + Dir.path().string() + "\" is not a directory, skipping");
        } else {
            TLuaPluginConfig Config { Path.stem().string() };
            FindAndParseConfig(Path, Config);
            InitializePlugin(Path, Config);
        }
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
        auto DataPtr = std::make_unique<StateThreadData>(Name, mShutdown, StateId, *this);
        mLuaStates[StateId] = std::move(DataPtr);
        RegisterEvent("onInit", StateId, "onInit");
        if (!DontCallOnInit) {
            auto Res = EnqueueFunctionCall(StateId, "onInit", {});
            Res->WaitUntilReady();
            if (Res->Error && Res->ErrorMessage != TLuaEngine::BeamMPFnNotFoundError) {
                beammp_lua_error("Calling \"onInit\" on \"" + StateId + "\" failed: " + Res->ErrorMessage);
            }
        }
    }
}

void TLuaEngine::RegisterEvent(const std::string& EventName, TLuaStateId StateId, const std::string& FunctionName) {
    std::unique_lock Lock(mLuaEventsMutex);
    mLuaEvents[EventName][StateId].insert(FunctionName);
}

std::set<std::string> TLuaEngine::GetEventHandlersForState(const std::string& EventName, TLuaStateId StateId) {
    return mLuaEvents[EventName][StateId];
}

sol::table TLuaEngine::StateThreadData::Lua_TriggerGlobalEvent(const std::string& EventName, sol::variadic_args EventArgs) {
    auto Return = mEngine->TriggerEvent(EventName, mStateId, EventArgs);
    // TODO Synchronous call to the event handlers
    auto MyHandlers = mEngine->GetEventHandlersForState(EventName, mStateId);
    for (const auto& Handler : MyHandlers) {
        auto Fn = mStateView[Handler];
        if (Fn.valid()) {
            auto LuaResult = Fn(EventArgs);
            auto Result = std::make_shared<TLuaResult>();
            Result->Ready = true;
            if (LuaResult.valid()) {
                Result->Error = false;
                Result->Result = LuaResult;
            } else {
                Result->Error = true;
                Result->ErrorMessage = "Function result in TriggerGlobalEvent was invalid";
            }
            Return.push_back(Result);
        }
    }
    sol::state_view StateView(mState);
    sol::table AsyncEventReturn = StateView.create_table();
    AsyncEventReturn["ReturnValueImpl"] = Return;
    AsyncEventReturn.set_function("IsDone",
        [&](const sol::table& Self) -> bool {
            auto Vector = Self.get<std::vector<std::shared_ptr<TLuaResult>>>("ReturnValueImpl");
            for (const auto& Value : Vector) {
                if (!Value->Ready) {
                    return false;
                }
            }
            return true;
        });
    AsyncEventReturn.set_function("GetResults",
        [&](const sol::table& Self) -> sol::table {
            sol::state_view StateView(mState);
            sol::table Result = StateView.create_table();
            auto Vector = Self.get<std::vector<std::shared_ptr<TLuaResult>>>("ReturnValueImpl");
            for (const auto& Value : Vector) {
                if (!Value->Ready) {
                    return sol::lua_nil;
                }
                Result.add(Value->Result);
            }
            return Result;
        });
    return AsyncEventReturn;
}

sol::table TLuaEngine::StateThreadData::Lua_TriggerLocalEvent(const std::string& EventName, sol::variadic_args EventArgs) {
    // TODO: make asynchronous?
    sol::table Result = mStateView.create_table();
    for (const auto& Handler : mEngine->GetEventHandlersForState(EventName, mStateId)) {
        auto Fn = mStateView[Handler];
        if (Fn.valid() && Fn.get_type() == sol::type::function) {
            auto FnRet = Fn(EventArgs);
            if (FnRet.valid()) {
                Result.add(FnRet);
            } else {
                sol::error Err = FnRet;
                beammp_lua_error(Err.what());
            }
        }
    }
    return Result;
}

sol::table TLuaEngine::StateThreadData::Lua_GetPlayerIdentifiers(int ID) {
    auto MaybeClient = GetClient(mEngine->Server(), ID);
    if (MaybeClient && !MaybeClient.value().expired()) {
        auto IDs = MaybeClient.value().lock()->GetIdentifiers();
        if (IDs.empty()) {
            return sol::lua_nil;
        }
        sol::table Result = mStateView.create_table();
        for (const auto& Pair : IDs) {
            Result[Pair.first] = Pair.second;
        }
        return Result;
    } else {
        return sol::lua_nil;
    }
}

sol::table TLuaEngine::StateThreadData::Lua_GetPlayers() {
    sol::table Result = mStateView.create_table();
    mEngine->Server().ForEachClient([&](std::weak_ptr<TClient> Client) -> bool {
        if (!Client.expired()) {
            auto locked = Client.lock();
            Result[locked->GetID()] = locked->GetName();
        }
        return true;
    });
    return Result;
}

int TLuaEngine::StateThreadData::Lua_GetPlayerIDByName(const std::string& Name) {
    int Id = -1;
    mEngine->mServer->ForEachClient([&Id, &Name](std::weak_ptr<TClient> Client) -> bool {
        if (!Client.expired()) {
            auto locked = Client.lock();
            if (locked->GetName() == Name) {
                Id = locked->GetID();
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
    if (MaybeClient && !MaybeClient.value().expired()) {
        return MaybeClient.value().lock()->GetName();
    } else {
        return "";
    }
}

sol::table TLuaEngine::StateThreadData::Lua_GetPlayerVehicles(int ID) {
    auto MaybeClient = GetClient(mEngine->Server(), ID);
    if (MaybeClient && !MaybeClient.value().expired()) {
        auto Client = MaybeClient.value().lock();
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
            Result[v.ID()] = v.Data().substr(3);
        }
        return Result;
    } else
        return sol::lua_nil;
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

static void JsonDecodeRecursive(sol::state_view& StateView, sol::table& table, const std::string& left, const nlohmann::json& right) {
    switch (right.type()) {
    case nlohmann::detail::value_t::null:
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

TLuaEngine::StateThreadData::StateThreadData(const std::string& Name, std::atomic_bool& Shutdown, TLuaStateId StateId, TLuaEngine& Engine)
    : mName(Name)
    , mShutdown(Shutdown)
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
    MPTable.set_function("SendChatMessage", &LuaAPI::MP::SendChatMessage);
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
    MPTable.set_function("Sleep", &LuaAPI::MP::Sleep);
    MPTable.set_function("CreateEventTimer", [&](const std::string& EventName, size_t IntervalMS) {
        if (IntervalMS < 25) {
            beammp_warn("Timer for \"" + EventName + "\" on \"" + mStateId + "\" is set to trigger at <25ms, which is likely too fast and won't cancel properly.");
        }
        mEngine->CreateEventTimer(EventName, mStateId, IntervalMS);
    });
    MPTable.set_function("CancelEventTimer", [&](const std::string& EventName) {
        mEngine->CancelEventTimers(EventName, mStateId);
    });
    MPTable.set_function("Set", &LuaAPI::MP::Set);
    MPTable.set_function("JsonEncode", &LuaAPI::MP::JsonEncode);
    MPTable.set_function("JsonDecode", [this](const std::string& str) {
        return Lua_JsonDecode(str);
    });
    MPTable.set_function("JsonDiff", &LuaAPI::MP::JsonDiff);

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
        "Description", 6);

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

std::shared_ptr<TLuaResult> TLuaEngine::StateThreadData::EnqueueScript(const TLuaChunk& Script) {
    std::unique_lock Lock(mStateExecuteQueueMutex);
    auto Result = std::make_shared<TLuaResult>();
    mStateExecuteQueue.push({ Script, Result });
    return Result;
}

std::shared_ptr<TLuaResult> TLuaEngine::StateThreadData::EnqueueFunctionCall(const std::string& FunctionName, const std::vector<TLuaArgTypes>& Args) {
    auto Result = std::make_shared<TLuaResult>();
    Result->StateId = mStateId;
    Result->Function = FunctionName;
    std::unique_lock Lock(mStateFunctionQueueMutex);
    mStateFunctionQueue.push({ FunctionName, Result, Args });
    mStateFunctionQueueCond.notify_all();
    return Result;
}

void TLuaEngine::StateThreadData::RegisterEvent(const std::string& EventName, const std::string& FunctionName) {
    mEngine->RegisterEvent(EventName, mStateId, FunctionName);
}

void TLuaEngine::StateThreadData::operator()() {
    RegisterThread("Lua:" + mStateId);
    while (!mShutdown) {
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
                S.second->Ready = true;
                if (Res.valid()) {
                    S.second->Error = false;
                    S.second->Result = std::move(Res);
                } else {
                    S.second->Error = true;
                    sol::error Err = Res;
                    S.second->ErrorMessage = Err.what();
                }
            }
        }
        { // StateFunctionQueue Scope
            std::unique_lock Lock(mStateFunctionQueueMutex);
            auto NotExpired = mStateFunctionQueueCond.wait_for(Lock,
                std::chrono::milliseconds(500),
                [&]() -> bool { return !mStateFunctionQueue.empty(); });
            if (NotExpired) {
                auto FnNameResultPair = std::move(mStateFunctionQueue.front());
                mStateFunctionQueue.pop();
                Lock.unlock();
                auto& FnName = std::get<0>(FnNameResultPair);
                auto& Result = std::get<1>(FnNameResultPair);
                auto Args = std::get<2>(FnNameResultPair);
                Result->StateId = mStateId;
                sol::state_view StateView(mState);
                auto Fn = StateView[FnName];
                if (Fn.valid() && Fn.get_type() == sol::type::function) {
                    std::vector<sol::object> LuaArgs;
                    for (const auto& Arg : Args) {
                        if (Arg.valueless_by_exception()) {
                            continue;
                        }
                        switch (Arg.index()) {
                        case TLuaArgTypes_String:
                            LuaArgs.push_back(sol::make_object(StateView, std::get<std::string>(Arg)));
                            break;
                        case TLuaArgTypes_Int:
                            LuaArgs.push_back(sol::make_object(StateView, std::get<int>(Arg)));
                            break;
                        case TLuaArgTypes_VariadicArgs:
                            LuaArgs.push_back(sol::make_object(StateView, std::get<sol::variadic_args>(Arg)));
                            break;
                        case TLuaArgTypes_Bool:
                            LuaArgs.push_back(sol::make_object(StateView, std::get<bool>(Arg)));
                            break;
                        default:
                            beammp_error("Unknown argument type, passed as nil");
                            break;
                        }
                    }
                    auto Res = Fn(sol::as_args(LuaArgs));
                    if (Res.valid()) {
                        Result->Error = false;
                        Result->Result = std::move(Res);
                    } else {
                        Result->Error = true;
                        sol::error Err = Res;
                        Result->ErrorMessage = Err.what();
                    }
                    Result->Ready = true;
                } else {
                    Result->Error = true;
                    Result->ErrorMessage = BeamMPFnNotFoundError; // special error kind that we can ignore later
                    Result->Ready = true;
                }
            }
        }
    }
}

std::queue<std::pair<TLuaChunk, std::shared_ptr<TLuaResult>>> TLuaEngine::StateThreadData::Debug_GetStateExecuteQueue() {
    std::unique_lock Lock(mStateExecuteQueueMutex);
    return mStateExecuteQueue;
}

std::queue<std::tuple<std::string, std::shared_ptr<TLuaResult>, std::vector<TLuaArgTypes>>> TLuaEngine::StateThreadData::Debug_GetStateFunctionQueue() {
    std::unique_lock Lock(mStateFunctionQueueMutex);
    return mStateFunctionQueue;
}

void TLuaEngine::CreateEventTimer(const std::string& EventName, TLuaStateId StateId, size_t IntervalMS) {
    std::unique_lock Lock(mTimedEventsMutex);
    TimedEvent Event {
        std::chrono::high_resolution_clock::duration { std::chrono::milliseconds(IntervalMS) },
        std::chrono::high_resolution_clock::now(),
        EventName,
        StateId
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

void TLuaResult::WaitUntilReady() {
    while (!Ready) {
        std::this_thread::yield();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
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

TPluginMonitor::TPluginMonitor(const fs::path& Path, TLuaEngine& Engine, std::atomic_bool& Shutdown)
    : mEngine(Engine)
    , mPath(Path)
    , mShutdown(Shutdown) {
    if (!fs::exists(mPath)) {
        fs::create_directories(mPath);
    }
    for (const auto& Entry : fs::recursive_directory_iterator(mPath)) {
        // TODO: trigger an event when a subfolder file changes
        if (Entry.is_regular_file()) {
            mFileTimes[Entry.path().string()] = fs::last_write_time(Entry.path());
        }
    }
    Start();
}

void TPluginMonitor::operator()() {
    RegisterThread("PluginMonitor");
    beammp_info("PluginMonitor started");
    while (!mShutdown) {
        std::this_thread::sleep_for(std::chrono::seconds(3));
        for (const auto& Pair : mFileTimes) {
            auto CurrentTime = fs::last_write_time(Pair.first);
            if (CurrentTime != Pair.second) {
                mFileTimes[Pair.first] = CurrentTime;
                // grandparent of the path should be Resources/Server
                if (fs::equivalent(fs::path(Pair.first).parent_path().parent_path(), mPath)) {
                    beammp_info("File \"" + Pair.first + "\" changed, reloading");
                    // is in root folder, so reload
                    std::ifstream FileStream(Pair.first, std::ios::in | std::ios::binary);
                    auto Size = std::filesystem::file_size(Pair.first);
                    auto Contents = std::make_shared<std::string>();
                    Contents->resize(Size);
                    FileStream.read(Contents->data(), Contents->size());
                    TLuaChunk Chunk(Contents, Pair.first, fs::path(Pair.first).parent_path().string());
                    auto StateID = mEngine.GetStateIDForPlugin(fs::path(Pair.first).parent_path());
                    auto Res = mEngine.EnqueueScript(StateID, Chunk);
                    // TODO: call onInit
                    mEngine.AddResultToCheck(Res);
                } else {
                    // TODO: trigger onFileChanged event
                    beammp_trace("Change detected in file \"" + Pair.first + "\", event trigger not implemented yet");
                    /*
                    // is in subfolder, dont reload, just trigger an event
                    auto Results = mEngine.TriggerEvent("onFileChanged", "", Pair.first);
                    mEngine.WaitForAll(Results);
                    for (const auto& Result : Results)  {
                        if (Result->Error) {
                            beammp_lua_error(Result->ErrorMessage);
                        }
                    }*/
                }
            }
        }
    }
}
