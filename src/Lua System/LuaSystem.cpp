///
/// Created by Anonymous275 on 5/19/2020
///

#include "../Network 2.0/Client.hpp"
#include "LuaSystem.hpp"
#include "../logger.h"
#include <iostream>
#include <thread>

LuaArg* CreateArg(lua_State *L,int T){
    LuaArg* temp = new LuaArg;
    for(int C = 2;C <= T;C++){
        if(lua_isstring(L,C)){
            temp->args.emplace_back(std::string(lua_tostring(L,C)));
        }else if(lua_isinteger(L,C)){
            temp->args.emplace_back(int(lua_tointeger(L,C)));
        }else if(lua_isboolean(L,C)){
            temp->args.emplace_back(bool(lua_toboolean(L,C)));
        }else if(lua_isnumber(L,C)) {
            temp->args.emplace_back(float(lua_tonumber(L, C)));
        }
    }
    return temp;
}

int TriggerLuaEvent(const std::string& Event,bool local,Lua*Caller,LuaArg* arg){
    int R = 0;
    for(Lua*Script : PluginEngine){
        if(Script->IsRegistered(Event)){
            if(local){
                if (Script->GetPluginName() == Caller->GetPluginName()){
                    R += Script->CallFunction(Script->GetRegistered(Event),arg);
                }
            }else R += Script->CallFunction(Script->GetRegistered(Event),arg);
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
    }else SendError(L,"RegisterEvent invalid argument count expected 2 got " + std::to_string(Args));
    return 0;
}
int lua_TriggerEventL(lua_State *L)
{
    int Args = lua_gettop(L);
    Lua* Script = GetScript(L);
    if(Args > 0){
        if(lua_isstring(L,1)) {
            TriggerLuaEvent(lua_tostring(L, 1), true, Script, CreateArg(L,Args));
        }else{
            SendError(L,"TriggerLocalEvent wrong argument [1] need string");
        }
    }else{
        SendError(L,"TriggerLocalEvent not enough arguments expected 1 got 0");
    }
    return 0;
}

int lua_TriggerEventG(lua_State *L)
{
    int Args = lua_gettop(L);
    Lua* Script = GetScript(L);
    if(Args > 0){
        if(lua_isstring(L,1)) {
            TriggerLuaEvent(lua_tostring(L, 1), false, Script, CreateArg(L,Args));
        }else SendError(L,"TriggerGlobalEvent wrong argument [1] need string");
    }else{
        SendError(L,"TriggerGlobalEvent not enough arguments");
    }
    return 0;
}
void CallAsync(Lua* Script,const std::string&FuncName,LuaArg* args){
    Script->CallFunction(FuncName,args);
}
int lua_CreateThread(lua_State *L){
    int Args = lua_gettop(L);
    Lua* Script = GetScript(L);
    if(Args > 0){
        if(lua_isstring(L,1)) {
            std::thread Worker(CallAsync,Script,lua_tostring(L,1),CreateArg(L,Args));
            Worker.detach();
        }else SendError(L,"CreateThread wrong argument [1] need string");
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
int lua_GetAllPlayers(lua_State *L){
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
                lua_pushstring(L, a.second.substr(3).c_str());
                lua_settable(L,-3);
                i++;
            }
            if(c->GetAllCars().empty())return 0;
        }else return 0;
    }else{
        SendError(L,"GetPlayerVehicles not enough arguments");
        return 0;
    }
    return 1;
}
void Respond(Client*c, const std::string& MSG, bool Rel);
int lua_dropPlayer(lua_State *L){
    int Args = lua_gettop(L);
    if(lua_isnumber(L,1)){
        int ID = lua_tonumber(L, 1);
        Client*c = GetClient(ID);
        std::string Reason;
        if(Args > 1 && lua_isstring(L,2)){
            Reason = std::string(" Reason : ")+lua_tostring(L,2);
        }
        if(c != nullptr){
            Respond(c,"C:Server:You have been Kicked from the server!" + Reason,true);
            c->SetStatus(-2);
            closesocket(c->GetTCPSock());
        }
    }else SendError(L,"DropPlayer not enough arguments");
    return 0;
}
void SendToAll(Client*c, const std::string& Data, bool Self, bool Rel);
int lua_sendChat(lua_State *L){
    if(lua_isinteger(L,1) || lua_isnumber(L,1)){
        if(lua_isstring(L,2)){
            int ID = lua_tointeger(L,1);
            if(ID == -1){
                std::string Packet = "C:Server: " + std::string(lua_tostring(L, 1));
                SendToAll(nullptr,Packet,true,true);
            }else{
                Client*c = GetClient(ID);
                if(c != nullptr) {
                    std::string Packet = "C:Server: " + std::string(lua_tostring(L, 1));
                    Respond(c, Packet, true);
                }else SendError(L,"SendChatMessage invalid argument [1] invalid ID");
            }
        }else SendError(L,"SendChatMessage invalid argument [2] expected string");
    }else SendError(L,"SendChatMessage invalid argument [1] expected number");
    return 0;
}
int lua_RemoveVehicle(lua_State *L){
    int Args = lua_gettop(L);
    if(Args != 2){
        SendError(L,"RemoveVehicle invalid argument count expected 2 got " + std::to_string(Args));
        return 0;
    }
    if((lua_isinteger(L,1) || lua_isnumber(L,1)) && (lua_isinteger(L,2) || lua_isnumber(L,2))){
        int PID = lua_tointeger(L,1);
        int VID = lua_tointeger(L,2);
        Client *c = GetClient(PID);
        if(c != nullptr){
            if(!c->GetCarData(VID).empty()){
                std::string Destroy = "Od:" + std::to_string(PID)+"-"+std::to_string(VID);
                SendToAll(nullptr,Destroy,true,true);
                c->DeleteCar(VID);
            }
        }else SendError(L,"RemoveVehicle invalid Player ID");
    }else SendError(L,"RemoveVehicle invalid argument expected number");
    return 0;
}
int lua_HWID(lua_State *L){
    lua_pushinteger(L, -1);
    return 1;
}
void Lua::Init(){
    luaL_openlibs(luaState);
    lua_register(luaState,"TriggerGlobalEvent",lua_TriggerEventG);
    lua_register(luaState,"TriggerLocalEvent",lua_TriggerEventL);
    lua_register(luaState,"GetPlayerCount",lua_GetPlayerCount);
    lua_register(luaState,"isPlayerConnected",lua_isConnected);
    lua_register(luaState,"RegisterEvent",lua_RegisterEvent);
    lua_register(luaState,"GetPlayerName",lua_GetPlayerName);
    lua_register(luaState,"RemoveVehicle",lua_RemoveVehicle);
    lua_register(luaState,"GetPlayerDiscordID",lua_GetDID);
    lua_register(luaState,"GetPlayerVehicles",lua_GetCars);
    lua_register(luaState,"CreateThread",lua_CreateThread);
    lua_register(luaState,"SendChatMessage",lua_sendChat);
    lua_register(luaState,"GetPlayers",lua_GetAllPlayers);
    lua_register(luaState,"DropPlayer",lua_dropPlayer);
    lua_register(luaState,"GetPlayerHWID",lua_HWID);
    lua_register(luaState,"Sleep",lua_Sleep);
    Reload();
}
void Lua::Reload(){
    if(CheckLua(luaState,luaL_dofile(luaState,FileName.c_str()))){
        CallFunction("onInit",{});
    }
}
int Lua::CallFunction(const std::string&FuncName,LuaArg* Arg){
    lua_getglobal(luaState, FuncName.c_str());
    if (lua_isfunction(luaState, -1)) {
        int Size = 0;
        if(Arg != nullptr){
            Size = Arg->args.size();
            Arg->PushArgs(luaState);
        }
        if(CheckLua(luaState, lua_pcall(luaState, Size, 1, 0))){

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
void Lua::SetLastWrite(fs::file_time_type time){
   LastWrote = time;
}
fs::file_time_type Lua::GetLastWrite(){
    return LastWrote;
}
