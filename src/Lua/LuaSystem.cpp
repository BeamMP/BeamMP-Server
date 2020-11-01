///
/// Created by Anonymous275 on 5/19/2020
///

#include "Lua/LuaSystem.hpp"
#include "Security/Enc.h"
#include "Client.hpp"
#include "Settings.h"
#include "Network.h"
#include "Logger.h"
#include "UnixCompat.h"
#include <iostream>
#include <future>
#include <utility>

LuaArg* CreateArg(lua_State *L,int T,int S){
    if(S > T)return nullptr;
    auto* temp = new LuaArg;
    for(int C = S;C <= T;C++){
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
void ClearStack(lua_State *L){
    lua_settop(L,0);
}
Lua* GetScript(lua_State *L){
    for(Lua*Script : PluginEngine){
        if (Script->GetState() == L)return Script;
    }
    return nullptr;
}
void SendError(lua_State *L,const std::string&msg){
    Lua* S = GetScript(L);
    std::string a;
    if(S == nullptr)a = Sec("Console");
    else a = S->GetFileName().substr(S->GetFileName().find('\\'));
    warn(a + Sec(" | Incorrect Call of ") + msg);
}
int Trigger(Lua*lua,const std::string& R, LuaArg*arg){
    std::lock_guard<std::mutex> lockGuard(lua->Lock);
    std::packaged_task<int()> task([lua,R,arg]{return CallFunction(lua,R,arg);});
    std::future<int> f1 = task.get_future();
    std::thread t(std::move(task));
    t.detach();
    auto status = f1.wait_for(std::chrono::seconds(5));
    if(status != std::future_status::timeout)return f1.get();
    SendError(lua->GetState(),R + " took too long to respond");
    return 0;
}
int FutureWait(Lua*lua,const std::string& R, LuaArg*arg,bool Wait){
    std::packaged_task<int()> task([lua,R,arg]{return Trigger(lua,R,arg);});
    std::future<int> f1 = task.get_future();
    std::thread t(std::move(task));
    t.detach();
    int T = 0;
    if(Wait)T = 6;
    auto status = f1.wait_for(std::chrono::seconds(T));
    if(status != std::future_status::timeout)return f1.get();
    return 0;
}
int TriggerLuaEvent(const std::string& Event,bool local,Lua*Caller,LuaArg* arg,bool Wait){
    int R = 0;
    for(Lua*Script : PluginEngine){
        if(Script->IsRegistered(Event)){
            if(local){
                if (Script->GetPluginName() == Caller->GetPluginName()){
                    R += FutureWait(Script,Script->GetRegistered(Event),arg,Wait);
                }
            }else R += FutureWait(Script,Script->GetRegistered(Event), arg,Wait);
        }
    }
    return R;
}
bool ConsoleCheck(lua_State *L, int r){
    if (r != LUA_OK){
        std::string msg = lua_tostring(L, -1);
        warn(Sec("Console | ") + msg);
        return false;
    }
    return true;
}
bool CheckLua(lua_State *L, int r){
    if (r != LUA_OK){
        std::string msg = lua_tostring(L, -1);
        Lua * S = GetScript(L);
        std::string a = S->GetFileName().substr(S->GetFileName().find('\\'));
        warn(a + " | " + msg);
        return false;
    }
    return true;
}

int lua_RegisterEvent(lua_State *L){
    int Args = lua_gettop(L);
    Lua* Script = GetScript(L);
    if(Args == 2 && lua_isstring(L,1) && lua_isstring(L,2)){
        Script->RegisterEvent(lua_tostring(L,1),lua_tostring(L,2));
    }else SendError(L,Sec("RegisterEvent invalid argument count expected 2 got ") + std::to_string(Args));
    return 0;
}
int lua_TriggerEventL(lua_State *L){
    int Args = lua_gettop(L);
    Lua* Script = GetScript(L);
    if(Args > 0){
        if(lua_isstring(L,1)){
            TriggerLuaEvent(lua_tostring(L, 1), true, Script, CreateArg(L,Args,2),false);
        }else SendError(L,Sec("TriggerLocalEvent wrong argument [1] need string"));
    }else{
        SendError(L,Sec("TriggerLocalEvent not enough arguments expected 1 got 0"));
    }
    return 0;
}

int lua_TriggerEventG(lua_State *L){
    int Args = lua_gettop(L);
    Lua* Script = GetScript(L);
    if(Args > 0){
        if(lua_isstring(L,1)) {
            TriggerLuaEvent(lua_tostring(L, 1), false, Script, CreateArg(L,Args,2),false);
        }else SendError(L,Sec("TriggerGlobalEvent wrong argument [1] need string"));
    }else SendError(L,Sec("TriggerGlobalEvent not enough arguments"));
    return 0;
}

char* ThreadOrigin(Lua*lua){
    std::string T = "Thread in " + lua->GetFileName().substr(lua->GetFileName().find('\\'));
    char* Data = new char[T.size()+1];
    ZeroMemory(Data,T.size()+1);
    memcpy(Data, T.c_str(),T.size());
    return Data;
}
void SafeExecution(Lua* lua,const std::string& FuncName){
    lua_State* luaState = lua->GetState();
    lua_getglobal(luaState, FuncName.c_str());
    if(lua_isfunction(luaState, -1)) {
        char* Origin = ThreadOrigin(lua);
#ifdef __WIN32
        __try{
                int R = lua_pcall(luaState, 0, 0, 0);
                CheckLua(luaState, R);
        }__except(Handle(GetExceptionInformation(),Origin)){}
#else // unix
        int R = lua_pcall(luaState, 0, 0, 0);
        CheckLua(luaState, R);
#endif // __WIN32
        delete [] Origin;
    }
    ClearStack(luaState);
}

void ExecuteAsync(Lua* lua,const std::string& FuncName){
    std::lock_guard<std::mutex> lockGuard(lua->Lock);
    SafeExecution(lua,FuncName);
}
void CallAsync(Lua* lua,const std::string& Func,int U){
    lua->StopThread = false;
    int D = 1000 / U;
    while(!lua->StopThread){
        ExecuteAsync(lua,Func);
        std::this_thread::sleep_for(std::chrono::milliseconds(D));
    }
}
int lua_StopThread(lua_State *L){
    GetScript(L)->StopThread = true;
    return 0;
}
int lua_CreateThread(lua_State *L){
    int Args = lua_gettop(L);
    if(Args > 1){
        if(lua_isstring(L,1)) {
            std::string STR = lua_tostring(L,1);
            if(lua_isinteger(L,2) || lua_isnumber(L,2)){
                int U = int(lua_tointeger(L,2));
                if(U > 0 && U < 501){
                    std::thread t1(CallAsync,GetScript(L),STR,U);
                    t1.detach();
                }else SendError(L,Sec("CreateThread wrong argument [2] number must be between 1 and 500"));
            }else SendError(L,Sec("CreateThread wrong argument [2] need number"));
        }else SendError(L,Sec("CreateThread wrong argument [1] need string"));
    }else SendError(L,Sec("CreateThread not enough arguments"));
    return 0;
}
int lua_Sleep(lua_State *L){
    if(lua_isnumber(L,1)){
        int t = int(lua_tonumber(L, 1));
        std::this_thread::sleep_for(std::chrono::milliseconds(t));
    }else{
        SendError(L,Sec("Sleep not enough arguments"));
        return 0;
    }
    return 1;
}
Client* GetClient(int ID){
    for(Client*c : CI->Clients) {
        if(c != nullptr && c->GetID() == ID)return c;
    }
    return nullptr;
}
int lua_isConnected(lua_State *L){
    if(lua_isnumber(L,1)){
        int ID = int(lua_tonumber(L, 1));
        Client*c = GetClient(ID);
        if(c != nullptr)lua_pushboolean(L, c->isConnected);
        else return 0;
    }else{
        SendError(L,Sec("isConnected not enough arguments"));
        return 0;
    }
    return 1;
}
int lua_GetPlayerName(lua_State *L){
    if(lua_isnumber(L,1)){
        int ID = int(lua_tonumber(L, 1));
        Client*c = GetClient(ID);
        if(c != nullptr)lua_pushstring(L, c->GetName().c_str());
        else return 0;
    }else{
        SendError(L,Sec("GetPlayerName not enough arguments"));
        return 0;
    }
    return 1;
}
int lua_GetPlayerCount(lua_State *L){
    lua_pushinteger(L, CI->Size());
    return 1;
}
int lua_GetDID(lua_State *L){
    if(lua_isnumber(L,1)){
        int ID = int(lua_tonumber(L, 1));
        Client*c = GetClient(ID);
        if(c != nullptr)lua_pushstring(L, c->GetDID().c_str());
        else return 0;
    }else{
        SendError(L,Sec("GetDID not enough arguments"));
        return 0;
    }
    return 1;
}
int lua_GetAllPlayers(lua_State *L){
    lua_newtable(L);
    int i = 1;
    for (Client *c : CI->Clients) {
        if(c == nullptr)continue;
        lua_pushinteger(L, c->GetID());
        lua_pushstring(L, c->GetName().c_str());
        lua_settable(L,-3);
        i++;
    }
    if(CI->Clients.empty())return 0;
    return 1;
}
int lua_GetCars(lua_State *L){
    if(lua_isnumber(L,1)){
        int ID = int(lua_tonumber(L, 1));
        Client*c = GetClient(ID);
        if(c != nullptr){
            int i = 1;
            for (VData*v : c->GetAllCars()) {
                if(v != nullptr) {
                    lua_pushinteger(L, v->ID);
                    lua_pushstring(L, v->Data.substr(3).c_str());
                    lua_settable(L, -3);
                    i++;
                }
            }
            if(c->GetAllCars().empty())return 0;
        }else return 0;
    }else{
        SendError(L,Sec("GetPlayerVehicles not enough arguments"));
        return 0;
    }
    return 1;
}
int lua_dropPlayer(lua_State *L){
    int Args = lua_gettop(L);
    if(lua_isnumber(L,1)){
        int ID = int(lua_tonumber(L, 1));
        Client*c = GetClient(ID);
        if(c == nullptr)return 0;
        if(c->GetRole() == Sec("MDEV"))return 0;
        std::string Reason;
        if(Args > 1 && lua_isstring(L,2)){
            Reason = std::string(Sec(" Reason : "))+lua_tostring(L,2);
        }
        Respond(c,"C:Server:You have been Kicked from the server! " + Reason,true);
        c->SetStatus(-2);
        closesocket(c->GetTCPSock());

    }else SendError(L,Sec("DropPlayer not enough arguments"));
    return 0;
}
int lua_sendChat(lua_State *L){
    if(lua_isinteger(L,1) || lua_isnumber(L,1)){
        if(lua_isstring(L,2)){
            int ID = int(lua_tointeger(L,1));
            if(ID == -1){
                std::string Packet = "C:Server: " + std::string(lua_tostring(L, 2));
                SendToAll(nullptr,Packet,true,true);
            }else{
                Client*c = GetClient(ID);
                if(c != nullptr) {
                    if(!c->isSynced)return 0;
                    std::string Packet ="C:Server: " + std::string(lua_tostring(L, 2));
                    Respond(c, Packet, true);
                }else SendError(L,Sec("SendChatMessage invalid argument [1] invalid ID"));
            }
        }else SendError(L,Sec("SendChatMessage invalid argument [2] expected string"));
    }else SendError(L,Sec("SendChatMessage invalid argument [1] expected number"));
    return 0;
}
int lua_RemoveVehicle(lua_State *L){
    int Args = lua_gettop(L);
    if(Args != 2){
        SendError(L,Sec("RemoveVehicle invalid argument count expected 2 got ") + std::to_string(Args));
        return 0;
    }
    if((lua_isinteger(L,1) || lua_isnumber(L,1)) && (lua_isinteger(L,2) || lua_isnumber(L,2))){
        int PID = int(lua_tointeger(L,1));
        int VID = int(lua_tointeger(L,2));
        Client *c = GetClient(PID);
        if(c == nullptr){
            SendError(L,Sec("RemoveVehicle invalid Player ID"));
            return 0;
        }
        if(c->GetRole() == "MDEV")return 0;
        if(!c->GetCarData(VID).empty()){
            std::string Destroy = "Od:" + std::to_string(PID)+"-"+std::to_string(VID);
            SendToAll(nullptr,Destroy,true,true);
            c->DeleteCar(VID);
        }
    }else SendError(L,Sec("RemoveVehicle invalid argument expected number"));
    return 0;
}
int lua_HWID(lua_State *L){
    lua_pushinteger(L, -1);
    return 1;
}
int lua_RemoteEvent(lua_State *L){
    int Args = lua_gettop(L);
    if(Args != 3){
        SendError(L,Sec("TriggerClientEvent invalid argument count expected 3 got ") + std::to_string(Args));
        return 0;
    }
    if(!lua_isnumber(L,1)){
        SendError(L,Sec("TriggerClientEvent invalid argument [1] expected number"));
        return 0;
    }
    if(!lua_isstring(L,2)){
        SendError(L,Sec("TriggerClientEvent invalid argument [2] expected string"));
        return 0;
    }
    if(!lua_isstring(L,3)){
        SendError(L,Sec("TriggerClientEvent invalid argument [3] expected string"));
        return 0;
    }
    int ID = int(lua_tointeger(L,1));
    std::string Packet = "E:" + std::string(lua_tostring(L,2)) + ":" + std::string(lua_tostring(L,3));
    if(ID == -1)SendToAll(nullptr,Packet,true,true);
    else{
        Client *c = GetClient(ID);
        if(c == nullptr){
            SendError(L,Sec("TriggerClientEvent invalid Player ID"));
            return 0;
        }
        Respond(c,Packet,true);
    }
    return 0;
}
int lua_ServerExit(lua_State *L){
    exit(0);
}
int lua_Set(lua_State *L){
    int Args = lua_gettop(L);
    if(Args != 2){
        SendError(L,Sec("set invalid argument count expected 2 got ") + std::to_string(Args));
        return 0;
    }
    if(!lua_isnumber(L,1)){
        SendError(L,Sec("set invalid argument [1] expected number"));
        return 0;
    }
    Lua* Src = GetScript(L);
    std::string Name;
    if(Src == nullptr)Name = Sec("Console");
    else Name = Src->GetPluginName();
    int C = int(lua_tointeger(L,1));
    switch(C){
        case 0: //debug
            if(lua_isboolean(L,2)){
                Debug = lua_toboolean(L,2);
                info(Name + Sec(" | Debug -> ") + (Debug?"true":"false"));
            }
            else SendError(L,Sec("set invalid argument [2] expected boolean for ID : 0"));
            break;
        case 1: //private
            if(lua_isboolean(L,2)){
                Private = lua_toboolean(L,2);
                info(Name + Sec(" | Private -> ") + (Private?"true":"false"));
            }
            else SendError(L,Sec("set invalid argument [2] expected boolean for ID : 1"));
            break;
        case 2: //max cars
            if(lua_isnumber(L,2)){
                MaxCars = int(lua_tointeger(L,2));
                info(Name + Sec(" | MaxCars -> ") + std::to_string(MaxCars));
            }
            else SendError(L,Sec("set invalid argument [2] expected number for ID : 2"));
            break;
        case 3: //max players
            if(lua_isnumber(L,2)){
                MaxPlayers = int(lua_tointeger(L,2));
                info(Name + Sec(" | MaxPlayers -> ") + std::to_string(MaxPlayers));
            }
            else SendError(L,Sec("set invalid argument [2] expected number for ID : 3"));
            break;
        case 4: //Map
            if(lua_isstring(L,2)){
                MapName = lua_tostring(L,2);
                info(Name + Sec(" | MapName -> ") + MapName);
            }
            else SendError(L,Sec("set invalid argument [2] expected string for ID : 4"));
            break;
        case 5: //Name
            if(lua_isstring(L,2)){
                ServerName = lua_tostring(L,2);
                info(Name + Sec(" | ServerName -> ") + ServerName);
            }
            else SendError(L,Sec("set invalid argument [2] expected string for ID : 5"));
            break;
        case 6: //Desc
            if(lua_isstring(L,2)){
                ServerDesc = lua_tostring(L,2);
                info(Name + Sec(" | ServerDesc -> ") + ServerDesc);
            }
            else SendError(L,Sec("set invalid argument [2] expected string for ID : 6"));
            break;
        default:
            warn(Sec("Invalid config ID : ") + std::to_string(C));
            break;
    }

    return 0;
}
extern "C" {
    int lua_Print(lua_State *L) {
        int Arg = lua_gettop(L);
        for (int i = 1; i <= Arg; i++){
           ConsoleOut(lua_tostring(L, i) + std::string("\n"));
        }
        return 0;
    }
}

void Lua::Init(){
    luaL_openlibs(luaState);
    lua_register(luaState,"TriggerGlobalEvent",lua_TriggerEventG);
    lua_register(luaState,"TriggerLocalEvent",lua_TriggerEventL);
    lua_register(luaState,"TriggerClientEvent",lua_RemoteEvent);
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
    lua_register(luaState,"StopThread",lua_StopThread);
    lua_register(luaState,"DropPlayer",lua_dropPlayer);
    lua_register(luaState,"GetPlayerHWID",lua_HWID);
    lua_register(luaState,"exit",lua_ServerExit);
    lua_register(luaState,"Sleep",lua_Sleep);
    lua_register(luaState,"print",lua_Print);
    lua_register(luaState,"Set",lua_Set);
    if(!Console)Reload();
}
void Lua::Execute(const std::string& Command){
    if(ConsoleCheck(luaState,luaL_dostring(luaState,Command.c_str()))){
        lua_settop(luaState, 0);
    }
}
void Lua::Reload(){
    if(CheckLua(luaState,luaL_dofile(luaState,FileName.c_str()))){
        CallFunction(this,Sec("onInit"), nullptr);
    }
}
char* Lua::GetOrigin(){
    std::string T = GetFileName().substr(GetFileName().find('\\'));
    char* Data = new char[T.size()];
    ZeroMemory(Data,T.size());
    memcpy(Data,T.c_str(),T.size());
    return Data;
}
int CallFunction(Lua*lua,const std::string& FuncName,LuaArg* Arg){
    lua_State*luaState = lua->GetState();
    lua_getglobal(luaState, FuncName.c_str());
    if(lua_isfunction(luaState, -1)) {
        int Size = 0;
        if(Arg != nullptr){
            Size = int(Arg->args.size());
            Arg->PushArgs(luaState);
            delete Arg;
            Arg = nullptr;
        }
        int R = 0;
        char* Origin = lua->GetOrigin();
#ifdef __WIN32
        __try{
#endif // __WIN32
            R = lua_pcall(luaState, Size, 1, 0);
            if (CheckLua(luaState, R)){
                if (lua_isnumber(luaState, -1)) {
                    return int(lua_tointeger(luaState, -1));
                }
            }
#ifdef __WIN32
        }__except(Handle(GetExceptionInformation(),Origin)){}
#endif // __WIN32
        delete [] Origin;
    }
    ClearStack(luaState);
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
