///
/// Created by Anonymous275 on 5/19/2020
///
#include "../Network 2.0/Client.hpp"
#include "LuaSystem.hpp"
#include "../logger.h"
#include <iostream>
#include <thread>

int TriggerLuaEvent(const std::string& Event,bool local,Lua*Caller){
    int R = 0;
    for(Lua*Script : PluginEngine){
        if(Script->IsRegistered(Event)){
            if(local){
                if (Script->GetPluginName() == Caller->GetPluginName()){
                    R += Script->CallFunction(Script->GetRegistered(Event));
                }
            }else R += Script->CallFunction(Script->GetRegistered(Event));
        }
    }
    return R;
}
bool CheckLua(lua_State *L, int r)
{
    if (r != LUA_OK)
    {
        std::string errormsg = lua_tostring(L, -1);
        std::cout << errormsg << std::endl;
        return false;
    }
    return true;
}
Lua* GetScript(lua_State *L){
    for(Lua*Script : PluginEngine){
        if (Script->GetState() == L)return Script;
    }
    return nullptr;
}
void SendError(lua_State *L,const std::string&msg){
    Lua* Script = GetScript(L);
    error(Script->GetFileName() + " | Incorrect Call of " +msg);
}
int lua_RegisterEvent(lua_State *L)
{
    int Args = lua_gettop(L);
    Lua* Script = GetScript(L);
    if(Args == 2 && lua_isstring(L,1) && lua_isstring(L,2)){
        Script->RegisterEvent(lua_tostring(L,1),lua_tostring(L,2));
    }else if(Args > 2){
        SendError(L,"RegisterEvent too many arguments");
    }else if(Args < 2){
        SendError(L,"RegisterEvent not enough arguments");
    }
    return 0;
}
int lua_TriggerEventL(lua_State *L)
{
    int Args = lua_gettop(L);
    Lua* Script = GetScript(L);
    if(Args > 0){
        if(lua_isstring(L,1)) {
            TriggerLuaEvent(lua_tostring(L, 1), true, Script);
        }else{
            SendError(L,"TriggerLocalEvent wrong arguments need string");
        }
    }else{
        SendError(L,"TriggerLocalEvent not enough arguments");
    }
    return 0;
}

int lua_TriggerEventG(lua_State *L)
{
    int Args = lua_gettop(L);
    Lua* Script = GetScript(L);
    if(Args > 0){
        if(lua_isstring(L,1)) {
            TriggerLuaEvent(lua_tostring(L, 1), true, Script);
        }else SendError(L,"TriggerGlobalEvent wrong arguments need string");
    }else{
        SendError(L,"TriggerGlobalEvent not enough arguments");
    }
    return 0;
}
void CallAsync(Lua* Script,const std::string&FuncName){
    Script->CallFunction(FuncName);
}
int lua_CreateThread(lua_State *L){
    int Args = lua_gettop(L);
    Lua* Script = GetScript(L);
    if(Args > 0){
        if(lua_isstring(L,1)) {
            std::thread Worker(CallAsync,Script,lua_tostring(L,1));
            Worker.detach();
        }else SendError(L,"CreateThread wrong arguments need string");
    }else SendError(L,"CreateThread not enough arguments");
    return 0;
}
int lua_Sleep(lua_State *L){
    if(lua_isnumber(L,1)){
        int t = lua_tonumber(L, 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(t));
    }else{
        SendError(L,"Sleep not enough arguments");
        return 0;
    }
    return 1;
}
Client* GetClient(int ID){
    for(Client*c:Clients) {
        if(c->GetID() == ID)return c;
    }
    return nullptr;
}
int lua_isConnected(lua_State *L)
{
    if(lua_isnumber(L,1)){
        int ID = lua_tonumber(L, 1);
        Client*c = GetClient(ID);
        if(c != nullptr)lua_pushboolean(L, c->isConnected());
        else return 0;
    }else{
        SendError(L,"CreateThread not enough arguments");
        return 0;
    }
    return 1;
}
int lua_GetPlayerName(lua_State *L){
    if(lua_isnumber(L,1)){
        int ID = lua_tonumber(L, 1);
        Client*c = GetClient(ID);
        if(c != nullptr)lua_pushstring(L, c->GetName().c_str());
        else return 0;
    }else{
        SendError(L,"CreateThread not enough arguments");
        return 0;
    }
    return 1;
}
int lua_GetPlayerCount(lua_State *L){
    lua_pushinteger(L, Clients.size());
    return 1;
}
int lua_GetDID(lua_State *L){
    if(lua_isnumber(L,1)){
        int ID = lua_tonumber(L, 1);
        Client*c = GetClient(ID);
        if(c != nullptr)lua_pushstring(L, c->GetDID().c_str());
        else return 0;
    }else{
        SendError(L,"GetDID not enough arguments");
        return 0;
    }
    return 1;
}
int lua_GetAllIDs(lua_State *L){
    lua_newtable(L);
    int i = 1;
    for (Client *c : Clients) {
        lua_pushinteger(L, c->GetID());
        lua_pushstring(L, c->GetName().c_str());
        lua_settable(L,-3);
        i++;
    }
    if(Clients.empty())return 0;
    return 1;
}
int lua_GetCars(lua_State *L){
    if(lua_isnumber(L,1)){
        int ID = lua_tonumber(L, 1);
        Client*c = GetClient(ID);

        if(c != nullptr){
            int i = 1;
            for (const std::pair<int,std::string> &a : c->GetAllCars()) {
                lua_pushinteger(L, a.first);
                lua_pushstring(L, a.second.c_str());
                lua_settable(L,-3);
                i++;
            }
        }else return 0;
    }else{
        SendError(L,"GetPlayerVehicles not enough arguments");
        return 0;
    }
    return 1;
}
void Respond(Client*c, const std::string& MSG, bool Rel);
int lua_dropPlayer(lua_State *L){
    if(lua_isnumber(L,1)){
        int ID = lua_tonumber(L, 1);
        Client*c = GetClient(ID);
        if(c != nullptr){
            Respond(c,"C:Server:You have been Kicked from the server!",true);
            c->SetStatus(-2);
        }
    }else SendError(L,"DropPlayer not enough arguments");
    return 0;
}
void SendToAll(Client*c, const std::string& Data, bool Self, bool Rel);
int lua_sendChat(lua_State *L){
    if(lua_isstring(L,1)){
        std::string Packet = "C:Server: " + std::string(lua_tostring(L, 1));
        SendToAll(nullptr,Packet,true,true);
    }else SendError(L,"SendChatMessage not enough arguments");
    return 0;
}
void Lua::Init(){
    luaL_openlibs(luaState);
    lua_register(luaState,"TriggerGlobalEvent",lua_TriggerEventG);
    lua_register(luaState,"TriggerLocalEvent",lua_TriggerEventL);
    lua_register(luaState,"GetPlayerCount",lua_GetPlayerCount);
    lua_register(luaState,"isPlayerConnected",lua_isConnected);
    lua_register(luaState,"RegisterEvent",lua_RegisterEvent);
    lua_register(luaState,"GetPlayerName",lua_GetPlayerName);
    lua_register(luaState,"GetPlayerVehicles",lua_GetCars);
    lua_register(luaState,"CreateThread",lua_CreateThread);
    lua_register(luaState,"SendChatMessage",lua_sendChat);
    lua_register(luaState,"DropPlayer",lua_dropPlayer);
    lua_register(luaState,"GetPlayers",lua_GetAllIDs);
    lua_register(luaState,"GetDID",lua_GetDID);
    lua_register(luaState,"Sleep",lua_Sleep);
    if(CheckLua(luaState,luaL_dofile(luaState,FileName.c_str()))){
        CallFunction("onInit");
    }
}
int Lua::CallFunction(const std::string&FuncName){
    lua_getglobal(luaState, FuncName.c_str());
    if (lua_isfunction(luaState, -1)) {
        /*lua_pushstring(luaState, "Anonymous275");
        lua_pushinteger(luaState, 1);*/
        if(CheckLua(luaState, lua_pcall(luaState, 0, 1, 0))){
            if(lua_isnumber(luaState,-1)){
                return lua_tointeger(luaState,-1);
            }
        }
    }
    return 0;
}
void Lua::SetPluginName(const std::string&Name){
    PluginName = Name;
}
void Lua::SetFileName(const std::string&Name){
    FileName = Name;
}
void Lua::RegisterEvent(const std::string& Event,const std::string& FunctionName){
    RegisteredEvents.insert(std::make_pair(Event,FunctionName));
}
void Lua::UnRegisterEvent(const std::string&Event){
    for(const std::pair<std::string,std::string>& a : RegisteredEvents){
        if(a.first == Event) {
            RegisteredEvents.erase(a);
            break;
        }
    }
}
bool Lua::IsRegistered(const std::string&Event){
    for(const std::pair<std::string,std::string>& a : RegisteredEvents){
        if(a.first == Event)return true;
    }
    return false;
}
std::string Lua::GetRegistered(const std::string&Event){
    for(const std::pair<std::string,std::string>& a : RegisteredEvents){
        if(a.first == Event)return a.second;
    }
    return "";
}
std::string Lua::GetFileName(){
    return FileName;
}
std::string Lua::GetPluginName(){
    return PluginName;
}
lua_State* Lua::GetState(){
    return luaState;
}
