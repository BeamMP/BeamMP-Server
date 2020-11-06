///
/// Created by Anonymous275 on 5/20/2020
///

#pragma once
#include "lua.hpp"
#include <any>
#include <filesystem>
#include <iostream>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

struct LuaArg {
    std::vector<std::any> args;
    void PushArgs(lua_State* State) {
        for (std::any arg : args) {
            if (!arg.has_value())
                return;
            std::string Type = arg.type().name();
            if (Type.find("bool") != std::string::npos) {
                lua_pushboolean(State, std::any_cast<bool>(arg));
            }
            if (Type.find("basic_string") != std::string::npos || Type.find("char") != std::string::npos) {
                lua_pushstring(State, std::any_cast<std::string>(arg).c_str());
            }
            if (Type.find("int") != std::string::npos) {
                lua_pushinteger(State, std::any_cast<int>(arg));
            }
            if (Type.find("float") != std::string::npos) {
                lua_pushnumber(State, std::any_cast<float>(arg));
            }
        }
    }
};

class Lua {
private:
    std::set<std::pair<std::string, std::string>> _RegisteredEvents;
    lua_State* luaState { nullptr };
    fs::file_time_type _LastWrote;
    std::string _PluginName;
    std::string _FileName;
    bool _StopThread = false;
    bool _Console = false;
    // this is called by the ctor to ensure RAII
    void Init();

public:
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
    Lua(const std::string& PluginName, const std::string& FileName, fs::file_time_type LastWrote, bool Console = false);
    Lua(bool Console = false);
    ~Lua();
    void SetStopThread(bool StopThread) { _StopThread = StopThread; }
    bool GetStopThread() const { return _StopThread; }
};
int CallFunction(Lua* lua, const std::string& FuncName, std::unique_ptr<LuaArg> args);
int TriggerLuaEvent(const std::string& Event, bool local, Lua* Caller, std::unique_ptr<LuaArg> arg, bool Wait);
extern std::set<std::unique_ptr<Lua>> PluginEngine;
