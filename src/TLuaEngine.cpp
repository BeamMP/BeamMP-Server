#include "TLuaEngine.h"
#include "Client.h"
#include "CustomAssert.h"
#include "Http.h"
#include "LuaAPI.h"
#include "TLuaPlugin.h"

#include <chrono>
#include <random>
#include <tuple>

static std::mt19937_64 MTGen64;

static TLuaStateId GenerateUniqueStateId() {
    auto Time = std::chrono::high_resolution_clock::now().time_since_epoch();
    return std::to_string(MTGen64()) + std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(Time).count());
}

TLuaEngine* LuaAPI::MP::Engine;

TLuaEngine::TLuaEngine() {
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
        mShutdown = true;
        if (mThread.joinable()) {
            mThread.join();
        }
    });
    Start();
}

void TLuaEngine::operator()() {
    RegisterThread("LuaEngine");
    // lua engine main thread
    CollectAndInitPlugins();
    // now call all onInit's
    auto Futures = TriggerEvent("onInit");
    WaitForAll(Futures);
    for (const auto& Future : Futures) {
        if (Future->Error && Future->ErrorMessage != BeamMPFnNotFoundError) {
            beammp_lua_error("Calling \"onInit\" on \"" + Future->StateId + "\" failed: " + Future->ErrorMessage);
        }
    }
    // this thread handles timers
    while (!mShutdown) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void TLuaEngine::WaitForAll(std::vector<std::shared_ptr<TLuaResult>>& Results) {
    for (const auto& Result : Results) {
        while (!Result->Ready) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

std::shared_ptr<TLuaResult> TLuaEngine::EnqueueScript(TLuaStateId StateID, const std::shared_ptr<std::string>& Script) {
    std::unique_lock Lock(mLuaStatesMutex);
    beammp_debug("enqueuing script into \"" + StateID + "\"");
    return mLuaStates.at(StateID)->EnqueueScript(Script);
}

std::shared_ptr<TLuaResult> TLuaEngine::EnqueueFunctionCall(TLuaStateId StateID, const std::string& FunctionName, const std::initializer_list<std::any>& Args) {
    std::unique_lock Lock(mLuaStatesMutex);
    beammp_debug("calling \"" + FunctionName + "\" in \"" + StateID + "\"");
    return mLuaStates.at(StateID)->EnqueueFunctionCall(FunctionName, Args);
}

void TLuaEngine::CollectAndInitPlugins() {
    for (const auto& Dir : fs::directory_iterator(mResourceServerPath)) {
        auto Path = Dir.path();
        Path = fs::relative(Path);
        if (!Dir.is_directory()) {
            beammp_error("\"" + Dir.path().string() + "\" is not a directory, skipping");
        } else {
            beammp_debug("found plugin directory: " + Path.string());
            TLuaPluginConfig Config { GenerateUniqueStateId() };
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
    TLuaPlugin Plugin(*this, Config, Folder);
}

void TLuaEngine::FindAndParseConfig(const fs::path& Folder, TLuaPluginConfig& Config) {
    auto ConfigFile = Folder / TLuaPluginConfig::FileName;
    if (fs::exists(ConfigFile) && fs::is_regular_file(ConfigFile)) {
        beammp_debug("\"" + ConfigFile.string() + "\" found");
        try {
            auto Data = toml::parse(ConfigFile);
            if (Data.contains("LuaStateID")) {
                auto ID = toml::find<std::string>(Data, "LuaStateID");
                if (!ID.empty()) {
                    beammp_debug("Plugin \"" + Folder.string() + "\" specified it wants LuaStateID \"" + ID + "\"");
                    Config.StateId = ID;
                } else {
                    beammp_debug("LuaStateID empty, using randomized state ID");
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
    std::unique_lock Lock(mEventsMutex);
    mEvents[EventName][StateId].insert(FunctionName);
}

std::set<std::string> TLuaEngine::GetEventHandlersForState(const std::string& EventName, TLuaStateId StateId) {
    return mEvents[EventName][StateId];
}

sol::table TLuaEngine::StateThreadData::Lua_TriggerGlobalEvent(const std::string& EventName) {
    auto Return = mEngine->TriggerEvent(EventName);
    beammp_debug("Triggering event \"" + EventName + "\" in \"" + mStateId + "\"");
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
                    return sol::nil;
                }
                Result.add(Value->Result);
            }
            return Result;
        });
    return AsyncEventReturn;
}

sol::table TLuaEngine::StateThreadData::Lua_TriggerLocalEvent(const std::string& EventName) {
    sol::table Result = mStateView.create_table();
    for (const auto& Handler : mEngine->GetEventHandlersForState(EventName, mStateId)) {
        auto Fn = mStateView[Handler];
        if (Fn.valid() && Fn.get_type() == sol::type::function) {
            Result.add(Fn());
        }
    }
    return Result;
}

sol::table TLuaEngine::StateThreadData::Lua_GetPlayerIdentifiers(int ID) {
    auto MaybeClient = GetClient(mEngine->Server(), ID);
    if (MaybeClient && !MaybeClient.value().expired()) {
        auto IDs = MaybeClient.value().lock()->GetIdentifiers();
        if (IDs.empty()) {
            return sol::nil;
        }
        sol::table Result = mStateView.create_table();
        for (const auto& Pair : IDs) {
            Result[Pair.first] = Pair.second;
        }
        return Result;
    } else {
        return sol::nil;
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
            return sol::nil;
        }
        sol::state_view StateView(mState);
        sol::table Result = StateView.create_table();
        for (const auto& v : VehicleData) {
            Result[v.ID()] = v.Data().substr(3);
        }
        return Result;
    } else
        return sol::nil;
}

TLuaEngine::StateThreadData::StateThreadData(const std::string& Name, std::atomic_bool& Shutdown, TLuaStateId StateId, TLuaEngine& Engine)
    : mName(Name)
    , mShutdown(Shutdown)
    , mStateId(StateId)
    , mState(luaL_newstate())
    , mEngine(&Engine) {
    luaL_openlibs(mState);
    sol::state_view StateView(mState);
    // StateView.globals()["package"].get()
    StateView.set_function("print", &LuaAPI::Print);
    StateView.set_function("exit", &Application::GracefullyShutdown);
    auto Table = StateView.create_named_table("MP");
    Table.set_function("CreateTimer", [&]() -> sol::table {
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
    Table.set_function("GetOSName", &LuaAPI::MP::GetOSName);
    Table.set_function("GetServerVersion", &LuaAPI::MP::GetServerVersion);
    Table.set_function("RegisterEvent", [this](const std::string& EventName, const std::string& FunctionName) {
        RegisterEvent(EventName, FunctionName);
    });
    Table.set_function("TriggerGlobalEvent", [&](const std::string& EventName) -> sol::table {
        return Lua_TriggerGlobalEvent(EventName);
    });
    Table.set_function("TriggerLocalEvent", [&](const std::string& EventName) -> sol::table {
        return Lua_TriggerLocalEvent(EventName);
    });
    Table.set_function("TriggerClientEvent", &LuaAPI::MP::TriggerClientEvent);
    Table.set_function("GetPlayerCount", &LuaAPI::MP::GetPlayerCount);
    Table.set_function("IsPlayerConnected", &LuaAPI::MP::IsPlayerConnected);
    Table.set_function("GetPlayerName", [&](int ID) -> std::string {
        return Lua_GetPlayerName(ID);
    });
    Table.set_function("RemoveVehicle", &LuaAPI::MP::RemoveVehicle);
    Table.set_function("GetPlayerVehicles", [&](int ID) -> sol::table {
        return Lua_GetPlayerVehicles(ID);
    });
    Table.set_function("SendChatMessage", &LuaAPI::MP::SendChatMessage);
    Table.set_function("GetPlayers", [&]() -> sol::table {
        return Lua_GetPlayers();
    });
    Table.set_function("IsPlayerGuest", &LuaAPI::MP::IsPlayerGuest);
    Table.set_function("DropPlayer", &LuaAPI::MP::DropPlayer);
    Table.set_function("GetPlayerIdentifiers", [&](int ID) -> sol::table {
        return Lua_GetPlayerIdentifiers(ID);
    });
    Table.set_function("Sleep", &LuaAPI::MP::Sleep);
    Table.set_function("Set", &LuaAPI::MP::Set);
    Table.set_function("HttpsGET", [&](const std::string& Host, int Port, const std::string& Target) -> std::tuple<int, std::string> {
        unsigned Status;
        auto Body = Http::GET(Host, Port, Target, &Status);
        return { Status, Body };
    });
    Table.set_function("HttpsPOST", [&](const std::string& Host, int Port, const std::string& Target, const std::string& Body, const std::string& ContentType) -> std::tuple<int, std::string> {
        unsigned Status;
        auto ResponseBody = Http::POST(Host, Port, Target, {}, Body, ContentType, &Status);
        return { Status, ResponseBody };
    });
    Table.create_named("Settings",
        "Debug", 0,
        "Private", 1,
        "MaxCars", 2,
        "MaxPlayers", 3,
        "Map", 4,
        "Name", 5,
        "Description", 6);
    Start();
}

std::shared_ptr<TLuaResult> TLuaEngine::StateThreadData::EnqueueScript(const std::shared_ptr<std::string>& Script) {
    beammp_debug("enqueuing script into \"" + mStateId + "\"");
    std::unique_lock Lock(mStateExecuteQueueMutex);
    auto Result = std::make_shared<TLuaResult>();
    mStateExecuteQueue.push({ Script, Result });
    return Result;
}

std::shared_ptr<TLuaResult> TLuaEngine::StateThreadData::EnqueueFunctionCall(const std::string& FunctionName, const std::initializer_list<std::any>& Args) {
    beammp_debug("calling \"" + FunctionName + "\" in \"" + mName + "\"");
    auto Result = std::make_shared<TLuaResult>();
    Result->StateId = mStateId;
    Result->Function = FunctionName;
    std::unique_lock Lock(mStateFunctionQueueMutex);
    mStateFunctionQueue.push({ FunctionName, Result, Args });
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

                beammp_debug("Running script");
                sol::state_view StateView(mState);
                auto Res = StateView.safe_script(*S.first, sol::script_pass_on_error);
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
            if (!mStateFunctionQueue.empty()) {
                auto FnNameResultPair = mStateFunctionQueue.front();
                mStateFunctionQueue.pop();
                Lock.unlock();
                auto& StateId = std::get<0>(FnNameResultPair);
                auto& Result = std::get<1>(FnNameResultPair);
                auto& Args = std::get<1>(FnNameResultPair);
                Result->StateId = mStateId;
                beammp_debug("Running function \"" + std::get<0>(FnNameResultPair) + "\"");
                sol::state_view StateView(mState);
                auto Fn = StateView[StateId];
                beammp_debug("Done running function \"" + StateId + "\"");
                if (Fn.valid() && Fn.get_type() == sol::type::function) {
                    auto Res = Fn(Args);
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
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void TLuaEngine::StateThreadData::AddPath(const fs::path& Path) {
    std::unique_lock Lock(mPathsMutex);
    mPaths.push(Path);
}

void TLuaResult::WaitUntilReady() {
    while (!Ready) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
