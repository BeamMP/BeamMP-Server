#ifndef TLUAFILE_H
#define TLUAFILE_H

#include "TLuaEngine.h"
#include <any>
#include <filesystem>
#include <lua.hpp>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct TLuaArg {
    std::vector<std::any> args;
    void PushArgs(lua_State* State);
};

class TLuaFile {
public:
    void Init();
    void RegisterEvent(const std::string& Event, const std::string& FunctionName);
    std::string GetRegistered(const std::string& Event) const;
    void UnRegisterEvent(const std::string& Event);
    void SetLastWrite(fs::file_time_type time);
    bool IsRegistered(const std::string& Event);
    void SetPluginName(const std::string& Name);
    void Execute(const std::string& Command);
    void SetFileName(const std::string& Name);
    fs::file_time_type GetLastWrite();
    std::string GetPluginName() const;
    std::string GetFileName() const;
    lua_State* GetState();
    const lua_State* GetState() const;
    std::string GetOrigin();
    std::mutex Lock;
    void Reload();
    TLuaFile(TLuaEngine& Engine, const std::string& PluginName, const std::string& FileName, fs::file_time_type LastWrote, bool Console = false);
    TLuaFile(TLuaEngine& Engine, bool Console = false);
    ~TLuaFile();
    void SetStopThread(bool StopThread) { _StopThread = StopThread; }
    bool GetStopThread() const { return _StopThread; }
    TLuaEngine& Engine() { return _Engine; }
    const TLuaEngine& Engine() const { return _Engine; }

private:
    TLuaEngine& _Engine;
    std::set<std::pair<std::string, std::string>> _RegisteredEvents;
    lua_State* luaState { nullptr };
    fs::file_time_type _LastWrote;
    std::string _PluginName {};
    std::string _FileName {};
    bool _StopThread = false;
    bool _Console = false;
};

#endif // TLUAFILE_H
