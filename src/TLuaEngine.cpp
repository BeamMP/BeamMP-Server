#include "TLuaEngine.h"
#include "CustomAssert.h"
#include "TLuaPlugin.h"

#include "LuaAPI.h"

#include <chrono>
#include <random>

static std::mt19937_64 MTGen64;

static TLuaStateId GenerateUniqueStateId() {
    auto Time = std::chrono::high_resolution_clock::now().time_since_epoch();
    return std::to_string(MTGen64()) + std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(Time).count());
}

TLuaEngine::TLuaEngine(TServer& Server, TNetwork& Network)
    : mNetwork(Network)
    , mServer(Server) {
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
    for (const auto& Pair : mLuaStates) {
        auto Res = EnqueueFunctionCall(Pair.first, "onInit");
        Res->WaitUntilReady();
        if (Res->Error && Res->ErrorMessage != TLuaEngine::BeamMPFnNotFoundError) {
            beammp_lua_error("Calling \"onInit\" on \"" + Pair.first + "\" failed: " + Res->ErrorMessage);
        }
    }
    // this thread handles timers
    while (!mShutdown) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

std::shared_ptr<TLuaResult> TLuaEngine::EnqueueScript(TLuaStateId StateID, const std::shared_ptr<std::string>& Script) {
    std::unique_lock Lock(mLuaStatesMutex);
    beammp_debug("enqueuing script into \"" + StateID + "\"");
    return mLuaStates.at(StateID)->EnqueueScript(Script);
}

std::shared_ptr<TLuaResult> TLuaEngine::EnqueueFunctionCall(TLuaStateId StateID, const std::string& FunctionName) {
    std::unique_lock Lock(mLuaStatesMutex);
    beammp_debug("calling \"" + FunctionName + "\" in \"" + StateID + "\"");
    return mLuaStates.at(StateID)->EnqueueFunctionCall(FunctionName);
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
    Lock.unlock();
    TLuaPlugin Plugin(*this, Config, Folder);
}

void TLuaEngine::FindAndParseConfig(const fs::path& Folder, TLuaPluginConfig& Config) {
    auto ConfigFile = Folder / TLuaPluginConfig::FileName;
    if (fs::exists(ConfigFile) && fs::is_regular_file(ConfigFile)) {
        beammp_debug("\"" + ConfigFile.string() + "\" found");
        // TODO use toml11 here to parse it
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
    if (mLuaStates.find(StateId) == mLuaStates.end()) {
        beammp_debug("Creating lua state for state id \"" + StateId + "\"");
        auto DataPtr = std::make_unique<StateThreadData>(Name, mShutdown, StateId);
        mLuaStates[StateId] = std::move(DataPtr);
        if (!DontCallOnInit) {
            auto Res = EnqueueFunctionCall(StateId, "onInit");
            Res->WaitUntilReady();
            if (Res->Error && Res->ErrorMessage != TLuaEngine::BeamMPFnNotFoundError) {
                beammp_lua_error("Calling \"onInit\" on \"" + StateId + "\" failed: " + Res->ErrorMessage);
            }
        }
    }
}

TLuaEngine::StateThreadData::StateThreadData(const std::string& Name, std::atomic_bool& Shutdown, TLuaStateId StateId)
    : mName(Name)
    , mShutdown(Shutdown)
    , mStateId(StateId) {
    mState = luaL_newstate();
    luaL_openlibs(mState);
    sol::state_view StateView(mState);
    StateView.set_function("print", &LuaAPI::Print);
    auto Table = StateView.create_named_table("MP");
    Table.set_function("GetOSName", &LuaAPI::MP::GetOSName);
    Table.set_function("GetServerVersion", &LuaAPI::MP::GetServerVersion);
    Start();
}

std::shared_ptr<TLuaResult> TLuaEngine::StateThreadData::EnqueueScript(const std::shared_ptr<std::string>& Script) {
    beammp_debug("enqueuing script into \"" + mStateId + "\"");
    std::unique_lock Lock(mStateExecuteQueueMutex);
    auto Result = std::make_shared<TLuaResult>();
    mStateExecuteQueue.push({ Script, Result });
    return Result;
}

std::shared_ptr<TLuaResult> TLuaEngine::StateThreadData::EnqueueFunctionCall(const std::string& FunctionName) {
    beammp_debug("calling \"" + FunctionName + "\" in \"" + mName + "\"");
    std::unique_lock Lock(mStateFunctionQueueMutex);
    auto Result = std::make_shared<TLuaResult>();
    mStateFunctionQueue.push({ FunctionName, Result });
    return Result;
}

void TLuaEngine::StateThreadData::operator()() {
    RegisterThread("Lua:" + mStateId);
    while (!mShutdown) {
        { // StateExecuteQueue Scope
            std::unique_lock Lock(mStateExecuteQueueMutex);
            if (mStateExecuteQueue.empty()) {
                Lock.unlock();
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            } else {
                auto S = mStateExecuteQueue.front();
                mStateExecuteQueue.pop();
                Lock.unlock();
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
            if (mStateFunctionQueue.empty()) {
                Lock.unlock();
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            } else {
                auto FnNameResultPair = mStateFunctionQueue.front();
                mStateFunctionQueue.pop();
                Lock.unlock();
                beammp_debug("Running function");
                sol::state_view StateView(mState);
                auto Fn = StateView[FnNameResultPair.first];
                if (Fn.valid() && Fn.get_type() == sol::type::function) {
                    auto Res = Fn();
                    FnNameResultPair.second->Ready = true;
                    if (Res.valid()) {
                        FnNameResultPair.second->Error = false;
                        FnNameResultPair.second->Result = std::move(Res);
                    } else {
                        FnNameResultPair.second->Error = true;
                        sol::error Err = Res;
                        FnNameResultPair.second->ErrorMessage = Err.what();
                    }
                } else {
                    FnNameResultPair.second->Ready = true;
                    FnNameResultPair.second->Error = true;
                    FnNameResultPair.second->ErrorMessage = BeamMPFnNotFoundError; // special error kind that we can ignore later
                }
            }
        }
    }
}

void TLuaResult::WaitUntilReady() {
    while (!Ready) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

// AHHH
std::any TriggerLuaEvent(const std::string& Event, bool local, TLuaPlugin* Caller, std::shared_ptr<TLuaArg> arg, bool Wait) {
}

void TLuaArg::PushArgs(lua_State* State) {
}
