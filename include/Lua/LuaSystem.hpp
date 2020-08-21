///
/// Created by Anonymous275 on 5/20/2020
///

#pragma once
#include <filesystem>
#include <iostream>
#include "lua.hpp"
#include <vector>
#include <thread>
#include <set>
#include <any>
namespace fs = std::experimental::filesystem;
struct LuaArg{
    std::vector<std::any> args;
    void PushArgs(lua_State *State){
       for(std::any arg : args){
            if(!arg.has_value())return;
            std::string Type = arg.type().name();
            if(Type.find("bool") != -1){
                lua_pushboolean(State,std::any_cast<bool>(arg));
            }
            if(Type.find("basic_string") != -1 || Type.find("char") != -1){
                lua_pushstring(State,std::any_cast<std::string>(arg).c_str());
            }
            if(Type.find("int") != -1){
                lua_pushinteger(State,std::any_cast<int>(arg));
            }
            if(Type.find("float") != -1){
                lua_pushnumber(State,std::any_cast<float>(arg));
            }
       }
    }
};

class Lua {
private:
    std::set<std::pair<std::string,std::string>> RegisteredEvents;
    lua_State *luaState = luaL_newstate();
    fs::file_time_type LastWrote;
    std::string PluginName;
    std::string FileName;

public:
    void RegisterEvent(const std::string&Event,const std::string&FunctionName);
    std::string GetRegistered(const std::string&Event);
    void UnRegisterEvent(const std::string&Event);
    void SetLastWrite(fs::file_time_type time);
    bool IsRegistered(const std::string&Event);
    void SetPluginName(const std::string&Name);
    void SetFileName(const std::string&Name);
    fs::file_time_type GetLastWrite();
    bool isThreadExecuting = false;
    std::string GetPluginName();
    std::string GetFileName();
    bool isExecuting = false;
    bool StopThread = false;
    bool HasThread = false;
    lua_State* GetState();
    char* GetOrigin();
    void Reload();
    void Init();
};
int CallFunction(Lua*lua,const std::string& FuncName,LuaArg* args);
int TriggerLuaEvent(const std::string& Event,bool local,Lua*Caller,LuaArg* arg);
extern std::set<Lua*> PluginEngine;