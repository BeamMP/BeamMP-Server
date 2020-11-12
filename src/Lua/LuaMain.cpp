///
/// Created by Anonymous275 on 5/20/2020
///

#include "Logger.h"
#include "Lua/LuaSystem.hpp"
#include "Security/Enc.h"
#include "Settings.h"
#include <thread>

#ifdef __linux
// we need this for `struct stat`
#include <sys/stat.h>
#endif // __linux

std::set<std::unique_ptr<Lua>> PluginEngine;
bool NewFile(const std::string& Path) {
    for (auto& Script : PluginEngine) {
        if (Path == Script->GetFileName())
            return false;
    }
    return true;
}
void RegisterFiles(const std::string& Path, bool HotSwap) {
    std::string Name = Path.substr(Path.find_last_of('\\') + 1);
    if (!HotSwap)
        info(Sec("Loading plugin : ") + Name);
    for (const auto& entry : fs::directory_iterator(Path)) {
        auto pos = entry.path().string().find(Sec(".lua"));
        if (pos != std::string::npos && entry.path().string().length() - pos == 4) {
            if (!HotSwap || NewFile(entry.path().string())) {
                auto FileName = entry.path().string();
                std::unique_ptr<Lua> ScriptToInsert(new Lua(Name, FileName, fs::last_write_time(FileName)));
                auto& Script = *ScriptToInsert;
                PluginEngine.insert(std::move(ScriptToInsert));
                if (HotSwap)
                    info(Sec("[HOTSWAP] Added : ") + Script.GetFileName().substr(Script.GetFileName().find('\\')));
            }
        }
    }
}
void FolderList(const std::string& Path, bool HotSwap) {
    for (const auto& entry : fs::directory_iterator(Path)) {
        auto pos = entry.path().filename().string().find('.');
        if (pos == std::string::npos) {
            RegisterFiles(entry.path().string(), HotSwap);
        }
    }
}
[[noreturn]] void HotSwaps(const std::string& path) {
    DebugPrintTID();
    while (true) {
        if (!PluginEngine.empty()) {
            for (auto& Script : PluginEngine) {
                struct stat Info { };
                if (stat(Script->GetFileName().c_str(), &Info) != 0) {
                    Script->SetStopThread(true);
                    PluginEngine.erase(Script);
                    info(Sec("[HOTSWAP] Removed : ") + Script->GetFileName().substr(Script->GetFileName().find('\\')));
                    break;
                }
                if (Script->GetLastWrite() != fs::last_write_time(Script->GetFileName())) {
                    Script->SetStopThread(true);
                    info(Sec("[HOTSWAP] Updated : ") + Script->GetFileName().substr(Script->GetFileName().find('\\')));
                    Script->SetLastWrite(fs::last_write_time(Script->GetFileName()));
                    Script->Reload();
                }
            }
        }
        FolderList(path, true);
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}

void InitLua() {
    if (!fs::exists(Resource)) {
        fs::create_directory(Resource);
    }
    std::string Path = Resource + Sec("/Server");
    if (!fs::exists(Path)) {
        fs::create_directory(Path);
    }
    FolderList(Path, false);
    std::thread t1(HotSwaps, Path);
    t1.detach();
    info(Sec("Lua system online"));
}
