#include "TLuaFile.h"
#include "Client.h"
#include "Common.h"
#include "CustomAssert.h"
#include "TLuaEngine.h"
#include "TServer.h"
#include "TTCPServer.h"
#include "TUDPServer.h"

#include <future>
#include <thread>

// TODO: REWRITE

void SendError(TLuaEngine& Engine, lua_State* L, const std::string& msg);
std::any CallFunction(TLuaFile* lua, const std::string& FuncName, std::shared_ptr<TLuaArg> Arg);
std::any TriggerLuaEvent(TLuaEngine& Engine, const std::string& Event, bool local, TLuaFile* Caller, std::shared_ptr<TLuaArg> arg, bool Wait);

extern TLuaEngine* TheEngine;

static TLuaEngine& Engine() {
    Assert(TheEngine);
    return *TheEngine;
}

std::shared_ptr<TLuaArg> CreateArg(lua_State* L, int T, int S) {
    if (S > T)
        return nullptr;
    std::shared_ptr<TLuaArg> temp(new TLuaArg);
    for (int C = S; C <= T; C++) {
        if (lua_isstring(L, C)) {
            temp->args.emplace_back(std::string(lua_tostring(L, C)));
        } else if (lua_isinteger(L, C)) {
            temp->args.emplace_back(int(lua_tointeger(L, C)));
        } else if (lua_isboolean(L, C)) {
            temp->args.emplace_back(bool(lua_toboolean(L, C)));
        } else if (lua_isnumber(L, C)) {
            temp->args.emplace_back(float(lua_tonumber(L, C)));
        }
    }
    return temp;
}
void ClearStack(lua_State* L) {
    lua_settop(L, 0);
}

std::any Trigger(TLuaFile* lua, const std::string& R, std::shared_ptr<TLuaArg> arg) {
    std::lock_guard<std::mutex> lockGuard(lua->Lock);
    std::packaged_task<std::any(std::shared_ptr<TLuaArg>)> task([lua, R](std::shared_ptr<TLuaArg> arg) { return CallFunction(lua, R, arg); });
    std::future<std::any> f1 = task.get_future();
    std::thread t(std::move(task), arg);
    t.detach();
    auto status = f1.wait_for(std::chrono::seconds(5));
    if (status != std::future_status::timeout)
        return f1.get();
    SendError(lua->Engine(), lua->GetState(), R + " took too long to respond");
    return 0;
}
std::any FutureWait(TLuaFile* lua, const std::string& R, std::shared_ptr<TLuaArg> arg, bool Wait) {
    Assert(lua);
    std::packaged_task<std::any(std::shared_ptr<TLuaArg>)> task([lua, R](std::shared_ptr<TLuaArg> arg) { return Trigger(lua, R, arg); });
    std::future<std::any> f1 = task.get_future();
    std::thread t(std::move(task), arg);
    t.detach();
    int T = 0;
    if (Wait)
        T = 6;
    auto status = f1.wait_for(std::chrono::seconds(T));
    if (status != std::future_status::timeout)
        return f1.get();
    return 0;
}
std::any TriggerLuaEvent(const std::string& Event, bool local, TLuaFile* Caller, std::shared_ptr<TLuaArg> arg, bool Wait) {
    std::any R;
    std::string Type;
    int Ret = 0;
    for (auto& Script : Engine().LuaFiles()) {
        if (Script->IsRegistered(Event)) {
            if (local) {
                if (Script->GetPluginName() == Caller->GetPluginName()) {
                    R = FutureWait(Script.get(), Script->GetRegistered(Event), arg, Wait);
                    Type = R.type().name();
                    if (Type.find("int") != std::string::npos) {
                        if (std::any_cast<int>(R))
                            Ret++;
                    } else if (Event == "onPlayerAuth")
                        return R;
                }
            } else {
                R = FutureWait(Script.get(), Script->GetRegistered(Event), arg, Wait);
                Type = R.type().name();
                if (Type.find("int") != std::string::npos) {
                    if (std::any_cast<int>(R))
                        Ret++;
                } else if (Event == "onPlayerAuth")
                    return R;
            }
        }
    }
    return Ret;
}
bool ConsoleCheck(lua_State* L, int r) {
    if (r != LUA_OK) {
        std::string msg = lua_tostring(L, -1);
        warn(("_Console | ") + msg);
        return false;
    }
    return true;
}
bool CheckLua(lua_State* L, int r) {
    if (r != LUA_OK) {
        std::string msg = lua_tostring(L, -1);
        auto MaybeS = Engine().GetScript(L);
        if (MaybeS.has_value()) {
            TLuaFile& S = MaybeS.value();
            std::string a = fs::path(S.GetFileName()).filename().string();
            warn(a + " | " + msg);
            return false;
        }
        // What the fuck, what do we do?!
        AssertNotReachable();
    }
    return true;
}

int lua_RegisterEvent(lua_State* L) {
    int Args = lua_gettop(L);
    auto MaybeScript = Engine().GetScript(L);
    Assert(MaybeScript.has_value());
    TLuaFile& Script = MaybeScript.value();
    if (Args == 2 && lua_isstring(L, 1) && lua_isstring(L, 2)) {
        Script.RegisterEvent(lua_tostring(L, 1), lua_tostring(L, 2));
    } else
        SendError(Engine(), L, "RegisterEvent invalid argument count expected 2 got " + std::to_string(Args));
    return 0;
}
int lua_TriggerEventL(lua_State* L) {
    int Args = lua_gettop(L);
    auto MaybeScript = Engine().GetScript(L);
    Assert(MaybeScript.has_value());
    TLuaFile& Script = MaybeScript.value();
    if (Args > 0) {
        if (lua_isstring(L, 1)) {
            TriggerLuaEvent(lua_tostring(L, 1), true, &Script, CreateArg(L, Args, 2), false);
        } else
            SendError(Engine(), L, ("TriggerLocalEvent wrong argument [1] need string"));
    } else {
        SendError(Engine(), L, ("TriggerLocalEvent not enough arguments expected 1 got 0"));
    }
    return 0;
}

int lua_TriggerEventG(lua_State* L) {
    int Args = lua_gettop(L);
    auto MaybeScript = Engine().GetScript(L);
    Assert(MaybeScript.has_value());
    TLuaFile& Script = MaybeScript.value();
    if (Args > 0) {
        if (lua_isstring(L, 1)) {
            TriggerLuaEvent(lua_tostring(L, 1), false, &Script, CreateArg(L, Args, 2), false);
        } else
            SendError(Engine(), L, ("TriggerGlobalEvent wrong argument [1] need string"));
    } else
        SendError(Engine(), L, ("TriggerGlobalEvent not enough arguments"));
    return 0;
}

char* ThreadOrigin(TLuaFile* lua) {
    std::string T = "Thread in " + fs::path(lua->GetFileName()).filename().string();
    char* Data = new char[T.size() + 1];
    std::fill_n(Data, T.size() + 1, 0);
    memcpy(Data, T.c_str(), T.size());
    return Data;
}
void SafeExecution(TLuaEngine& Engine, TLuaFile* lua, const std::string& FuncName) {
    lua_State* luaState = lua->GetState();
    lua_getglobal(luaState, FuncName.c_str());
    if (lua_isfunction(luaState, -1)) {
        char* Origin = ThreadOrigin(lua);
#ifdef WIN32
        //__try {
        int R = lua_pcall(luaState, 0, 0, 0);
        CheckLua(luaState, R);
        /*} __except (Handle(GetExceptionInformation(), Origin)) {
        }*/
#else // unix
        int R = lua_pcall(luaState, 0, 0, 0);
        CheckLua(luaState, R);
#endif // WIN32
        delete[] Origin;
    }
    ClearStack(luaState);
}

void ExecuteAsync(TLuaEngine& Engine, TLuaFile* lua, const std::string& FuncName) {
    std::lock_guard<std::mutex> lockGuard(lua->Lock);
    SafeExecution(Engine, lua, FuncName);
}
void CallAsync(TLuaFile* lua, const std::string& Func, int U) {
    //DebugPrintTID();
    lua->SetStopThread(false);
    int D = 1000 / U;
    while (!lua->GetStopThread()) {
        ExecuteAsync(Engine(), lua, Func);
        std::this_thread::sleep_for(std::chrono::milliseconds(D));
    }
}
int lua_StopThread(lua_State* L) {
    auto MaybeScript = Engine().GetScript(L);
    Assert(MaybeScript.has_value());
    // ugly, but whatever, this is safe as fuck
    MaybeScript.value().get().SetStopThread(true);
    return 0;
}
int lua_CreateThread(lua_State* L) {
    int Args = lua_gettop(L);
    if (Args > 1) {
        if (lua_isstring(L, 1)) {
            std::string STR = lua_tostring(L, 1);
            if (lua_isinteger(L, 2) || lua_isnumber(L, 2)) {
                int U = int(lua_tointeger(L, 2));
                if (U > 0 && U < 501) {
                    auto MaybeScript = Engine().GetScript(L);
                    Assert(MaybeScript.has_value());
                    TLuaFile& Script = MaybeScript.value();
                    std::thread t1(CallAsync, &Script, STR, U);
                    t1.detach();
                } else
                    SendError(Engine(), L, ("CreateThread wrong argument [2] number must be between 1 and 500"));
            } else
                SendError(Engine(), L, ("CreateThread wrong argument [2] need number"));
        } else
            SendError(Engine(), L, ("CreateThread wrong argument [1] need string"));
    } else
        SendError(Engine(), L, ("CreateThread not enough arguments"));
    return 0;
}
int lua_Sleep(lua_State* L) {
    if (lua_isnumber(L, 1)) {
        int t = int(lua_tonumber(L, 1));
        std::this_thread::sleep_for(std::chrono::milliseconds(t));
    } else {
        SendError(Engine(), L, ("Sleep not enough arguments"));
        return 0;
    }
    return 1;
}
std::optional<std::weak_ptr<TClient>> GetClient(TServer& Server, int ID) {
    std::optional<std::weak_ptr<TClient>> MaybeClient { std::nullopt };
    Server.ForEachClient([&](std::weak_ptr<TClient> CPtr) -> bool {
        if (!CPtr.expired()) {
            auto C = CPtr.lock();
            if (C->GetID() == ID) {
                MaybeClient = CPtr;
                return false;
            }
        }
        return true;
    });
    return MaybeClient;
}
// CONTINUE
int lua_isConnected(lua_State* L) {
    if (lua_isnumber(L, 1)) {
        int ID = int(lua_tonumber(L, 1));
        auto MaybeClient = GetClient(Engine().Server(), ID);
        if (MaybeClient && !MaybeClient.value().expired())
            lua_pushboolean(L, MaybeClient.value().lock()->IsConnected());
        else
            return 0;
    } else {
        SendError(Engine(), L, ("isConnected not enough arguments"));
        return 0;
    }
    return 1;
}
int lua_GetPlayerName(lua_State* L) {
    if (lua_isnumber(L, 1)) {
        int ID = int(lua_tonumber(L, 1));
        auto MaybeClient = GetClient(Engine().Server(), ID);
        if (MaybeClient && !MaybeClient.value().expired())
            lua_pushstring(L, MaybeClient.value().lock()->GetName().c_str());
        else
            return 0;
    } else {
        SendError(Engine(), L, ("GetPlayerName not enough arguments"));
        return 0;
    }
    return 1;
}
int lua_GetPlayerCount(lua_State* L) {
    lua_pushinteger(L, Engine().Server().ClientCount());
    return 1;
}
int lua_GetGuest(lua_State* L) {
    if (lua_isnumber(L, 1)) {
        int ID = int(lua_tonumber(L, 1));
        auto MaybeClient = GetClient(Engine().Server(), ID);
        if (MaybeClient && !MaybeClient.value().expired())
            lua_pushboolean(L, MaybeClient.value().lock()->IsGuest());
        else
            return 0;
    } else {
        SendError(Engine(), L, "GetGuest not enough arguments");
        return 0;
    }
    return 1;
}
int lua_GetAllPlayers(lua_State* L) {
    lua_newtable(L);
    Engine().Server().ForEachClient([&](const std::weak_ptr<TClient>& ClientPtr) -> bool {
        if (ClientPtr.expired())
            return true;
        auto Client = ClientPtr.lock();
        lua_pushinteger(L, Client->GetID());
        lua_pushstring(L, Client->GetName().c_str());
        lua_settable(L, -3);
        return true;
    });
    if (Engine().Server().ClientCount() == 0)
        return 0;
    return 1;
}
int lua_GetCars(lua_State* L) {
    if (lua_isnumber(L, 1)) {
        int ID = int(lua_tonumber(L, 1));
        auto MaybeClient = GetClient(Engine().Server(), ID);
        if (MaybeClient && !MaybeClient.value().expired()) {
            auto Client = MaybeClient.value().lock();
            int i = 1;
            TClient::TSetOfVehicleData VehicleData;
            { // Vehicle Data Lock Scope
                auto LockedData = Client->GetAllCars();
                VehicleData = LockedData.VehicleData;
            } // End Vehicle Data Lock Scope
            for (auto& v : VehicleData) {
                lua_pushinteger(L, v.ID());
                lua_pushstring(L, v.Data().substr(3).c_str());
                lua_settable(L, -3);
                i++;
            }
            if (VehicleData.empty())
                return 0;
        } else
            return 0;
    } else {
        SendError(Engine(), L, ("GetPlayerVehicles not enough arguments"));
        return 0;
    }
    return 1;
}
int lua_dropPlayer(lua_State* L) {
    int Args = lua_gettop(L);
    if (lua_isnumber(L, 1)) {
        int ID = int(lua_tonumber(L, 1));
        auto MaybeClient = GetClient(Engine().Server(), ID);
        if (!MaybeClient || MaybeClient.value().expired())
            return 0;
        std::string Reason;
        if (Args > 1 && lua_isstring(L, 2)) {
            Reason = std::string((" Reason : ")) + lua_tostring(L, 2);
        }
        auto c = MaybeClient.value().lock();
        Engine().TCPServer().Respond(*c, "C:Server:You have been Kicked from the server! " + Reason, true);
        c->SetStatus(-2);
        info(("Closing socket due to kick"));
        CloseSocketProper(c->GetTCPSock());
    } else
        SendError(Engine(), L, ("DropPlayer not enough arguments"));
    return 0;
}
int lua_sendChat(lua_State* L) {
    if (lua_isinteger(L, 1) || lua_isnumber(L, 1)) {
        if (lua_isstring(L, 2)) {
            int ID = int(lua_tointeger(L, 1));
            if (ID == -1) {
                std::string Packet = "C:Server: " + std::string(lua_tostring(L, 2));
                Engine().UDPServer().SendToAll(nullptr, Packet, true, true);
            } else {
                auto MaybeClient = GetClient(Engine().Server(), ID);
                if (MaybeClient && !MaybeClient.value().expired()) {
                    auto c = MaybeClient.value().lock();
                    if (!c->IsSynced())
                        return 0;
                    std::string Packet = "C:Server: " + std::string(lua_tostring(L, 2));
                    Engine().TCPServer().Respond(*c, Packet, true);
                } else
                    SendError(Engine(), L, ("SendChatMessage invalid argument [1] invalid ID"));
            }
        } else
            SendError(Engine(), L, ("SendChatMessage invalid argument [2] expected string"));
    } else
        SendError(Engine(), L, ("SendChatMessage invalid argument [1] expected number"));
    return 0;
}
int lua_RemoveVehicle(lua_State* L) {
    int Args = lua_gettop(L);
    if (Args != 2) {
        SendError(Engine(), L, ("RemoveVehicle invalid argument count expected 2 got ") + std::to_string(Args));
        return 0;
    }
    if ((lua_isinteger(L, 1) || lua_isnumber(L, 1)) && (lua_isinteger(L, 2) || lua_isnumber(L, 2))) {
        int PID = int(lua_tointeger(L, 1));
        int VID = int(lua_tointeger(L, 2));
        auto MaybeClient = GetClient(Engine().Server(), PID);
        if (!MaybeClient || MaybeClient.value().expired()) {
            SendError(Engine(), L, ("RemoveVehicle invalid Player ID"));
            return 0;
        }
        auto c = MaybeClient.value().lock();
        if (!c->GetCarData(VID).empty()) {
            std::string Destroy = "Od:" + std::to_string(PID) + "-" + std::to_string(VID);
            Engine().UDPServer().SendToAll(nullptr, Destroy, true, true);
            c->DeleteCar(VID);
        }
    } else
        SendError(Engine(), L, ("RemoveVehicle invalid argument expected number"));
    return 0;
}
int lua_HWID(lua_State* L) {
    lua_pushinteger(L, -1);
    return 1;
}
int lua_RemoteEvent(lua_State* L) {
    int Args = lua_gettop(L);
    if (Args != 3) {
        SendError(Engine(), L, ("TriggerClientEvent invalid argument count expected 3 got ") + std::to_string(Args));
        return 0;
    }
    if (!lua_isnumber(L, 1)) {
        SendError(Engine(), L, ("TriggerClientEvent invalid argument [1] expected number"));
        return 0;
    }
    if (!lua_isstring(L, 2)) {
        SendError(Engine(), L, ("TriggerClientEvent invalid argument [2] expected string"));
        return 0;
    }
    if (!lua_isstring(L, 3)) {
        SendError(Engine(), L, ("TriggerClientEvent invalid argument [3] expected string"));
        return 0;
    }
    int ID = int(lua_tointeger(L, 1));
    std::string Packet = "E:" + std::string(lua_tostring(L, 2)) + ":" + std::string(lua_tostring(L, 3));
    if (ID == -1)
        Engine().UDPServer().SendToAll(nullptr, Packet, true, true);
    else {
        auto MaybeClient = GetClient(Engine().Server(), ID);
        if (!MaybeClient || MaybeClient.value().expired()) {
            SendError(Engine(), L, ("TriggerClientEvent invalid Player ID"));
            return 0;
        }
        auto c = MaybeClient.value().lock();
        Engine().TCPServer().Respond(*c, Packet, true);
    }
    return 0;
}
int lua_ServerExit(lua_State* L) {
    if (lua_gettop(L) > 0) {
        if (lua_isnumber(L, 1)) {
            _Exit(int(lua_tointeger(L, 1)));
        }
    }
    _Exit(0);
}
int lua_Set(lua_State* L) {
    int Args = lua_gettop(L);
    if (Args != 2) {
        SendError(Engine(), L, ("set invalid argument count expected 2 got ") + std::to_string(Args));
        return 0;
    }
    if (!lua_isnumber(L, 1)) {
        SendError(Engine(), L, ("set invalid argument [1] expected number"));
        return 0;
    }
    auto MaybeSrc = Engine().GetScript(L);
    std::string Name;
    if (!MaybeSrc.has_value()) {
        Name = ("_Console");
    } else {
        Name = MaybeSrc.value().get().GetPluginName();
    }
    int C = int(lua_tointeger(L, 1));
    switch (C) {
    case 0: //debug
        if (lua_isboolean(L, 2)) {
            Application::Settings.DebugModeEnabled = lua_toboolean(L, 2);
            info(Name + (" | Debug -> ") + (Application::Settings.DebugModeEnabled ? "true" : "false"));
        } else
            SendError(Engine(), L, ("set invalid argument [2] expected boolean for ID : 0"));
        break;
    case 1: //private
        if (lua_isboolean(L, 2)) {
            Application::Settings.Private = lua_toboolean(L, 2);
            info(Name + (" | Private -> ") + (Application::Settings.Private ? "true" : "false"));
        } else
            SendError(Engine(), L, ("set invalid argument [2] expected boolean for ID : 1"));
        break;
    case 2: //max cars
        if (lua_isnumber(L, 2)) {
            Application::Settings.MaxCars = int(lua_tointeger(L, 2));
            info(Name + (" | MaxCars -> ") + std::to_string(Application::Settings.MaxCars));
        } else
            SendError(Engine(), L, ("set invalid argument [2] expected number for ID : 2"));
        break;
    case 3: //max players
        if (lua_isnumber(L, 2)) {
            Application::Settings.MaxPlayers = int(lua_tointeger(L, 2));
            info(Name + (" | MaxPlayers -> ") + std::to_string(Application::Settings.MaxPlayers));
        } else
            SendError(Engine(), L, ("set invalid argument [2] expected number for ID : 3"));
        break;
    case 4: //Map
        if (lua_isstring(L, 2)) {
            Application::Settings.MapName = lua_tostring(L, 2);
            info(Name + (" | MapName -> ") + Application::Settings.MapName);
        } else
            SendError(Engine(), L, ("set invalid argument [2] expected string for ID : 4"));
        break;
    case 5: //Name
        if (lua_isstring(L, 2)) {
            Application::Settings.ServerName = lua_tostring(L, 2);
            info(Name + (" | ServerName -> ") + Application::Settings.ServerName);
        } else
            SendError(Engine(), L, ("set invalid argument [2] expected string for ID : 5"));
        break;
    case 6: //Desc
        if (lua_isstring(L, 2)) {
            Application::Settings.ServerDesc = lua_tostring(L, 2);
            info(Name + (" | ServerDesc -> ") + Application::Settings.ServerDesc);
        } else
            SendError(Engine(), L, ("set invalid argument [2] expected string for ID : 6"));
        break;
    default:
        warn(("Invalid config ID : ") + std::to_string(C));
        break;
    }

    return 0;
}
extern "C" {
int lua_Print(lua_State* L) {
    int Arg = lua_gettop(L);
    std::string to_print;
    for (int i = 1; i <= Arg; i++) {
        if (lua_isstring(L, i)) {
            to_print += lua_tostring(L, i);
        } else if (lua_isinteger(L, i)) {
            to_print += std::to_string(lua_tointeger(L, 1));
        } else if (lua_isnumber(L, i)) {
            to_print += std::to_string(lua_tonumber(L, 1));
        } else if (lua_isboolean(L, i)) {
            to_print += lua_toboolean(L, i) ? "true" : "false";
        } else if (lua_isfunction(L, i)) {
            std::stringstream ss;
            ss << std::hex << reinterpret_cast<const void*>(lua_tocfunction(L, i));
            to_print += "function: " + ss.str();
        } else if (lua_istable(L, i)) {
            std::stringstream ss;
            ss << std::hex << reinterpret_cast<const void*>(lua_topointer(L, i));
            to_print += "table: " + ss.str();
        } else if (lua_isnoneornil(L, i)) {
            to_print += "nil";
        } else if (lua_isthread(L, i)) {
            std::stringstream ss;
            ss << std::hex << reinterpret_cast<const void*>(lua_tothread(L, i));
            to_print += "thread: " + ss.str();
        } else {
            to_print += "(unknown)";
        }
        if (i + 1 <= Arg) {
            to_print += "\t";
        }
    }
    luaprint(to_print);
    return 0;
}
}

int lua_TempFix(lua_State* L);

void TLuaFile::Init(const std::string& PluginName, const std::string& FileName, fs::file_time_type LastWrote) {
    // set global engine for lua_* functions
    if (!TheEngine) {
        TheEngine = &mEngine;
    }
    Assert(mLuaState);
    if (!PluginName.empty()) {
        SetPluginName(PluginName);
    }
    if (!FileName.empty()) {
        SetFileName(FileName);
    }
    SetLastWrite(LastWrote);
    Load();
}

TLuaFile::TLuaFile(TLuaEngine& Engine, bool Console)
    : mEngine(Engine)
    , mLuaState(luaL_newstate()) {
    if (Console) {
        mConsole = Console;
        Load();
    }
}

void TLuaFile::Execute(const std::string& Command) {
    if (ConsoleCheck(mLuaState, luaL_dostring(mLuaState, Command.c_str()))) {
        lua_settop(mLuaState, 0);
    }
}
void TLuaFile::Reload() {
    if (CheckLua(mLuaState, luaL_dofile(mLuaState, mFileName.c_str()))) {
        CallFunction(this, ("onInit"), nullptr);
    }
}
std::string TLuaFile::GetOrigin() {
    return fs::path(GetFileName()).filename().string();
}

std::any CallFunction(TLuaFile* lua, const std::string& FuncName, std::shared_ptr<TLuaArg> Arg) {
    lua_State* luaState = lua->GetState();
    lua_getglobal(luaState, FuncName.c_str());
    if (lua_isfunction(luaState, -1)) {
        int Size = 0;
        if (Arg != nullptr) {
            Size = int(Arg->args.size());
            Arg->PushArgs(luaState);
        }
        int R = lua_pcall(luaState, Size, 1, 0);
        if (CheckLua(luaState, R)) {
            if (lua_isnumber(luaState, -1)) {
                return int(lua_tointeger(luaState, -1));
            } else if (lua_isstring(luaState, -1)) {
                return std::string(lua_tostring(luaState, -1));
            }
        }
    }
    ClearStack(luaState);
    return 0;
}
void TLuaFile::SetPluginName(const std::string& Name) {
    mPluginName = Name;
}
void TLuaFile::SetFileName(const std::string& Name) {
    mFileName = Name;
}

void TLuaFile::Load() {
    Assert(mLuaState);
    luaL_openlibs(mLuaState);
    lua_register(mLuaState, "TriggerGlobalEvent", lua_TriggerEventG);
    lua_register(mLuaState, "TriggerLocalEvent", lua_TriggerEventL);
    lua_register(mLuaState, "TriggerClientEvent", lua_RemoteEvent);
    lua_register(mLuaState, "GetPlayerCount", lua_GetPlayerCount);
    lua_register(mLuaState, "isPlayerConnected", lua_isConnected);
    lua_register(mLuaState, "RegisterEvent", lua_RegisterEvent);
    lua_register(mLuaState, "GetPlayerName", lua_GetPlayerName);
    lua_register(mLuaState, "RemoveVehicle", lua_RemoveVehicle);
    lua_register(mLuaState, "GetPlayerDiscordID", lua_TempFix);
    lua_register(mLuaState, "CreateThread", lua_CreateThread);
    lua_register(mLuaState, "GetPlayerVehicles", lua_GetCars);
    lua_register(mLuaState, "SendChatMessage", lua_sendChat);
    lua_register(mLuaState, "GetPlayers", lua_GetAllPlayers);
    lua_register(mLuaState, "GetPlayerGuest", lua_GetGuest);
    lua_register(mLuaState, "StopThread", lua_StopThread);
    lua_register(mLuaState, "DropPlayer", lua_dropPlayer);
    lua_register(mLuaState, "GetPlayerHWID", lua_HWID);
    lua_register(mLuaState, "exit", lua_ServerExit);
    lua_register(mLuaState, "Sleep", lua_Sleep);
    lua_register(mLuaState, "print", lua_Print);
    lua_register(mLuaState, "Set", lua_Set);
    if (!mConsole)
        Reload();
}

void TLuaFile::RegisterEvent(const std::string& Event, const std::string& FunctionName) {
    mRegisteredEvents.insert(std::make_pair(Event, FunctionName));
}
void TLuaFile::UnRegisterEvent(const std::string& Event) {
    for (const std::pair<std::string, std::string>& a : mRegisteredEvents) {
        if (a.first == Event) {
            mRegisteredEvents.erase(a);
            break;
        }
    }
}
bool TLuaFile::IsRegistered(const std::string& Event) {
    for (const std::pair<std::string, std::string>& a : mRegisteredEvents) {
        if (a.first == Event)
            return true;
    }
    return false;
}
std::string TLuaFile::GetRegistered(const std::string& Event) const {
    for (const std::pair<std::string, std::string>& a : mRegisteredEvents) {
        if (a.first == Event)
            return a.second;
    }
    return "";
}
std::string TLuaFile::GetFileName() const {
    return mFileName;
}
std::string TLuaFile::GetPluginName() const {
    return mPluginName;
}
lua_State* TLuaFile::GetState() {
    return mLuaState;
}

const lua_State* TLuaFile::GetState() const {
    return mLuaState;
}

void TLuaFile::SetLastWrite(fs::file_time_type time) {
    mLastWrote = time;
}
fs::file_time_type TLuaFile::GetLastWrite() {
    return mLastWrote;
}

TLuaFile::~TLuaFile() {
    info("closing lua state");
    lua_close(mLuaState);
}

void SendError(TLuaEngine& Engine, lua_State* L, const std::string& msg) {
    Assert(L);
    auto MaybeS = Engine.GetScript(L);
    std::string a;
    if (!MaybeS.has_value()) {
        a = ("_Console");
    } else {
        TLuaFile& S = MaybeS.value();
        a = fs::path(S.GetFileName()).filename().string();
    }
    warn(a + (" | Incorrect Call of ") + msg);
}
int lua_TempFix(lua_State* L) {
    if (lua_isnumber(L, 1)) {
        int ID = int(lua_tonumber(L, 1));
        auto MaybeClient = GetClient(Engine().Server(), ID);
        if (!MaybeClient || MaybeClient.value().expired())
            return 0;
        std::string Ret;
        auto c = MaybeClient.value().lock();
        if (c->IsGuest()) {
            Ret = "Guest-" + c->GetName();
        } else
            Ret = c->GetName();
        lua_pushstring(L, Ret.c_str());
    } else
        SendError(Engine(), L, "GetDID not enough arguments");
    return 1;
}

void TLuaArg::PushArgs(lua_State* State) {
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
