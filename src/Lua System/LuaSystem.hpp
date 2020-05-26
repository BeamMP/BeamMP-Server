///
/// Created by Anonymous275 on 5/20/2020
///

#pragma once
#include <set>
#include "../lua/lua.hpp"
class Lua {
private:
    std::set<std::pair<std::string,std::string>> RegisteredEvents;
    lua_State *luaState = luaL_newstate();
    std::string PluginName;
    std::string FileName;

public:
    void RegisterEvent(const std::string&Event,const std::string&FunctionName);
    std::string GetRegistered(const std::string&Event);
    void UnRegisterEvent(const std::string&Event);
    int CallFunction(const std::string&FuncName);
    bool IsRegistered(const std::string&Event);
    void SetPluginName(const std::string&Name);
    void SetFileName(const std::string&Name);
    std::string GetPluginName();
    std::string GetFileName();
    lua_State* GetState();
    void Init();
};

extern std::set<Lua*> PluginEngine;