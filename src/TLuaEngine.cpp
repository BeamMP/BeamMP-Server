#include "TLuaEngine.h"
#include "CustomAssert.h"
#include "TLuaPlugin.h"

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
    CollectPlugins();
    while (!mShutdown) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

std::shared_ptr<TLuaResult> TLuaEngine::EnqueueScript(TLuaStateId StateID, const std::shared_ptr<std::string>& Script) {
    std::unique_lock Lock(mLuaStatesMutex);
    TLuaResult Result;
    beammp_debug("enqueuing script into \"" + StateID + "\"");
    return mLuaStates.at(StateID)->EnqueueScript(Script);
}

void TLuaEngine::CollectPlugins() {
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
    TLuaPlugin Plugin(*this, Config);
    std::unique_lock Lock(mLuaStatesMutex);
    EnsureStateExists(Config.StateId, Folder.stem().string());
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

void TLuaEngine::EnsureStateExists(TLuaStateId StateId, const std::string& Name) {
    if (mLuaStates.find(StateId) == mLuaStates.end()) {
        beammp_debug("Creating lua state for state id \"" + StateId + "\"");
        auto DataPtr = std::make_unique<StateThreadData>(Name, mShutdown);
        mLuaStates[StateId] = std::move(DataPtr);
    }
}

TLuaEngine::StateThreadData::StateThreadData(const std::string& Name, std::atomic_bool& Shutdown)
    : mName(Name)
    , mShutdown(Shutdown) {
    mState = luaL_newstate();
    luaL_openlibs(mState);
    Start();
}

std::shared_ptr<TLuaResult> TLuaEngine::StateThreadData::EnqueueScript(const std::shared_ptr<std::string>& Script) {
    beammp_debug("enqueuing script into \"" + mName + "\"");
    std::unique_lock Lock(mStateExecuteQueueMutex);
    auto Result = std::make_shared<TLuaResult>();
    mStateExecuteQueue.push({ Script, Result });
    return Result;
}

void TLuaEngine::StateThreadData::operator()() {
    RegisterThread(mName);
    while (!mShutdown) {
        std::unique_lock Lock(mStateExecuteQueueMutex);
        if (mStateExecuteQueue.empty()) {
            Lock.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        } else {
            auto S = mStateExecuteQueue.front();
            mStateExecuteQueue.pop();
            Lock.unlock();
            beammp_debug("Running script");
            luaL_dostring(mState, S.first->data());
            S.second->Ready = true;
        }
    }
}

TLuaEngine::StateThreadData::~StateThreadData() {
    if (mThread.joinable()) {
        mThread.join();
    }
}

// AHHH
std::any TriggerLuaEvent(const std::string& Event, bool local, TLuaPlugin* Caller, std::shared_ptr<TLuaArg> arg, bool Wait) {
}

void TLuaArg::PushArgs(lua_State* State) {
}
