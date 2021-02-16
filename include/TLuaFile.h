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
    void UnRegisterEvent(const std::string& Event);
    void SetLastWrite(fs::file_time_type time);
    bool IsRegistered(const std::string& Event);
    void SetPluginName(const std::string& Name);
    void Execute(const std::string& Command);
    void SetFileName(const std::string& Name);
    fs::file_time_type GetLastWrite();
    lua_State* GetState();
    std::string GetOrigin();
    std::mutex Lock;
    void Reload();
    TLuaFile(TLuaEngine& Engine, const std::string& PluginName, const std::string& FileName, fs::file_time_type LastWrote, bool Console = false);
    explicit TLuaFile(TLuaEngine& Engine, bool Console = false);
    ~TLuaFile();
    void SetStopThread(bool StopThread) { mStopThread = StopThread; }
    TLuaEngine& Engine() { return mEngine; }
    [[nodiscard]] std::string GetPluginName() const;
    [[nodiscard]] std::string GetFileName() const;
    [[nodiscard]] const lua_State* GetState() const;
    [[nodiscard]] bool GetStopThread() const { return mStopThread; }
    [[nodiscard]] const TLuaEngine& Engine() const { return mEngine; }
    [[nodiscard]] std::string GetRegistered(const std::string& Event) const;

private:
    TLuaEngine& mEngine;
    std::set<std::pair<std::string, std::string>> mRegisteredEvents;
    lua_State* mLuaState { nullptr };
    fs::file_time_type mLastWrote;
    std::string mPluginName {};
    std::string mFileName {};
    bool mStopThread = false;
    bool mConsole = false;
};

#endif // TLUAFILE_H
