#include "TLuaEngine.h"
#include "TLuaFile.h"

#include <filesystem>
#include <sys/stat.h>

namespace fs = std::filesystem;

std::unordered_map<std::string, lua_State*> TLuaEngine::mGlobals;

// necessary as lua relies on global state
TLuaEngine* TheEngine;

TLuaEngine::TLuaEngine(TServer& Server, TNetwork& Network)
    : mNetwork(Network)
    , mServer(Server) {
    TheEngine = this;
    if (!fs::exists(Application::Settings.Resource)) {
        fs::create_directory(Application::Settings.Resource);
    }
    std::string Path = Application::Settings.Resource + ("/Server");
    if (!fs::exists(Path)) {
        fs::create_directory(Path);
    }
    FolderList(Path, false);
    mPath = Path;
    Application::RegisterShutdownHandler([&] {if (mThread.joinable()) {
        debug("shutting down LuaEngine");
        mShutdown = true;
        mThread.join();
        debug("shut down LuaEngine");
    } });
    Start();
}

void TLuaEngine::operator()() {
    RegisterThread("LuaEngine");
    info("Lua system online");
    while (!mShutdown) {
        if (!mLuaFiles.empty()) {
            for (auto& Script : mLuaFiles) {
                struct stat Info { };
                if (stat(Script->GetFileName().c_str(), &Info) != 0) {
                    Script->SetStopThread(true);
                    mLuaFiles.erase(Script);
                    info(("[HOTSWAP] Removed removed script due to delete"));
                    break;
                }
                if (Script->GetLastWrite() != fs::last_write_time(Script->GetFileName())) {
                    Script->SetStopThread(true);
                    info(("[HOTSWAP] Updated Scripts due to edit"));
                    Script->SetLastWrite(fs::last_write_time(Script->GetFileName()));
                    Script->Reload();
                }
            }
        }
        FolderList(mPath, true);
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}

std::optional<std::reference_wrapper<TLuaFile>> TLuaEngine::GetScript(lua_State* L) {
    for (auto& Script : mLuaFiles) {
        if (Script->GetState() == L)
            return *Script;
    }
    return std::nullopt;
}

void TLuaEngine::FolderList(const std::string& Path, bool HotSwap) {
    for (const auto& entry : fs::directory_iterator(Path)) {
        auto pos = entry.path().filename().string().find('.');
        if (pos == std::string::npos) {
            RegisterFiles(entry.path().string(), HotSwap);
        }
    }
}

void TLuaEngine::RegisterFiles(const std::string& Path, bool HotSwap) {
#if defined(__linux) || defined(__linux__)
    std::string Name = Path.substr(Path.find_last_of('/') + 1);
#else
    std::string Name = Path.substr(Path.find_last_of('\\') + 1);
#endif
    if (!HotSwap)
        info(("Loading plugin : ") + Name);
    for (const auto& entry : fs::directory_iterator(Path)) {
        auto pos = entry.path().string().find((".lua"));
        if (pos != std::string::npos && entry.path().string().length() - pos == 4) {
            if (!HotSwap || NewFile(entry.path().string())) {
                auto FileName = entry.path().string();
                std::unique_ptr<TLuaFile> ScriptToInsert(new TLuaFile(*this));
                auto& Script = *ScriptToInsert;
                mLuaFiles.insert(std::move(ScriptToInsert));
                Script.Init(Name, FileName, fs::last_write_time(FileName));
                if (HotSwap)
                    info(("[HOTSWAP] Added : ") + Script.GetFileName().substr(Script.GetFileName().find('\\')));
            }
        }
    }
}

bool TLuaEngine::NewFile(const std::string& Path) {
    for (auto& Script : mLuaFiles) {
        if (Path == Script->GetFileName())
            return false;
    }
    return true;
}
