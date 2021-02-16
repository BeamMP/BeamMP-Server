#include "TLuaEngine.h"
#include "TLuaFile.h"

#include <filesystem>

namespace fs = std::filesystem;

TLuaEngine::TLuaEngine(TServer& Server)
    : mServer(Server) {
    if (!fs::exists(Application::Settings.ResourceFolder)) {
        fs::create_directory(Application::Settings.ResourceFolder);
    }
    std::string Path = Application::Settings.ResourceFolder + ("/Server");
    if (!fs::exists(Path)) {
        fs::create_directory(Path);
    }
    FolderList(Path, false);
}

void TLuaEngine::operator()() {
    info("Lua system online");
    // thread main
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
    std::string Name = Path.substr(Path.find_last_of('\\') + 1);
    if (!HotSwap)
        info(("Loading plugin : ") + Name);
    for (const auto& entry : fs::directory_iterator(Path)) {
        auto pos = entry.path().string().find((".lua"));
        if (pos != std::string::npos && entry.path().string().length() - pos == 4) {
            if (!HotSwap || NewFile(entry.path().string())) {
                auto FileName = entry.path().string();
                std::unique_ptr<TLuaFile> ScriptToInsert(new TLuaFile(*this, Name, FileName, fs::last_write_time(FileName)));
                auto& Script = *ScriptToInsert;
                mLuaFiles.insert(std::move(ScriptToInsert));
                Script.Init();
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
