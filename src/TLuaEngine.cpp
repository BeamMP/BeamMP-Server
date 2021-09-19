#include "TLuaEngine.h"
#include "Client.h"
#include "CustomAssert.h"
#include "Http.h"
#include "LuaAPI.h"
#include "TLuaPlugin.h"

#include <chrono>
#include <random>
#include <tuple>

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
    auto Futures = TriggerEvent("onInit", "");
    WaitForAll(Futures);
    for (const auto& Future : Futures) {
        if (Future->Error && Future->ErrorMessage != BeamMPFnNotFoundError) {
            beammp_lua_error("Calling \"onInit\" on \"" + Future->StateId + "\" failed: " + Future->ErrorMessage);
        }
    }
    // this thread handles timers
    while (!mShutdown) {
        std::this_thread::sleep_for(std::chrono::mi));
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

void TLuaEngine::WaitForAll(std::vector<std::shared_ptr<TLuaResult>>& Results) {
    for (const auto& Result : Results) {
        while (!Result->Ready) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
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
            TLuaPluginConfig Config { Path.string() };
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
                    return sol::nil;
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
                beammp_lua_error(sol::error(FnRet).what());
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
    if (!mState) {
        beammp_error("failed to create lua state for \"" + StateId + "\"");
        return;
    }
    luaL_openlibs(mState);
    sol::state_view StateView(mState);
    lua_atpanic(mState, LuaAPI::PanicHandler);
    // StateView.globals()["package"].get()
    StateView.set_function("print", &LuaAPI::Print);
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
    MPTable.set_function("PrintRaw", &LuaAPI::MP::PrintRaw);
    MPTable.set_function("Set", &LuaAPI::MP::Set);
    MPTable.set_function("HttpsGET", [&](const std::string& Host, int Port, const std::string& Target) -> std::tuple<int, std::string> {
        unsigned Status;
        auto Body = Http::GET(Host, Port, Target, &Status);
        return { Status, Body };
    });
    MPTable.set_function("HttpsPOST", [&](const std::string& Host, int Port, const std::string& Target, const std::string& Body, const std::string& ContentType) -> std::tuple<int, std::string> {
        unsigned Status;
        auto ResponseBody = Http::POST(Host, Port, Target, {}, Body, ContentType, &Status);
        return { Status, ResponseBody };
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
    FSTable.set_function("CreateDirectory", [&](const std::string& Path) -> std::pair<bool, std::string> {
        std::error_code errc;
        std::pair<bool, std::string> Result;
        fs::create_directories(Path, errc);
        Result.first = errc == std::error_code {};
        if (!Result.first) {
            Result.second = errc.message();
        }
        return Result;
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
            if (!mStateFunctionQueue.empty()) {
                auto FnNameResultPair = std::move(mStateFunctionQueue.front());
                mStateFunctionQueue.pop();
                Lock.unlock();
                auto& StateId = std::get<0>(FnNameResultPair);
                auto& Result = std::get<1>(FnNameResultPair);
                auto Args = std::get<2>(FnNameResultPair);
                Result->StateId = mStateId;
                sol::state_view StateView(mState);
                auto Fn = StateView[StateId];
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

TLuaChunk::TLuaChunk(std::shared_ptr<std::string> Content, std::string FileName, std::string PluginPath)
    : Content(Content)
    , FileName(FileName)
    , PluginPath(PluginPath) {
}
