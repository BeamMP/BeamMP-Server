#include "TLuaEngine.h"
#include "Client.h"
#include "CustomAssert.h"
#include "TLuaFile.h"

#include <lua.hpp>

extern TLuaEngine* TheLuaEngine;

TLuaEngine::TLuaEngine(TServer& Server, TNetwork& Network)
    : mNetwork(Network)
    , mServer(Server) {
    TheLuaEngine = this;
    if (!fs::exists(Application::Settings.Resource)) {
        fs::create_directory(Application::Settings.Resource);
    }
    std::string Path = Application::Settings.Resource + ("/Server");
    if (!fs::exists(Path)) {
        fs::create_directory(Path);
    }
    FolderList(Path, false);
    mPath = Path;
    Application::RegisterShutdownHandler([&] {if (mThread.joinable()) {
        debug("shutting down LuaEngine");
        mShutdown = true;
        mThread.join();
        debug("shut down LuaEngine");
    } });
    Start();
}

void TLuaEngine::UnregisterScript(std::shared_ptr<TLuaFile> Script) {
    std::unique_lock Lock(mLuaFilesMutex);
    auto Iter = std::find_if(mLuaFiles.begin(), mLuaFiles.end(), [&](const auto& ScriptPtr) {
        return *Script == *ScriptPtr;
    });
    if (Iter != mLuaFiles.end()) {
        // found!
        mLuaFiles.erase(Iter);
    }
}

void TLuaEngine::operator()() {
    RegisterThread("LuaEngine");
    info("Lua system online");
    while (!mShutdown) {
        if (!mLuaFiles.empty()) {
            for (auto& Script : mLuaFiles) {
                const auto& filename = Script->GetFileName();
                if (!fs::is_regular_file(filename)) {
                    Script->SetStopThread(true);
                    UnregisterScript(Script);
                    info(("[HOTSWAP] Removed removed script due to delete"));
                    break;
                }
                if (Script->GetLastWrite() != fs::last_write_time(Script->GetFileName())) {
                    Script->SetStopThread(true);
                    info(("[HOTSWAP] Updated Scripts due to edit"));
                    Script->SetLastWrite(fs::last_write_time(Script->GetFileName()));
                    Script->Reload();
                }
            }
        }
        FolderList(mPath, true);
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}

std::any TLuaEngine::TriggerLuaEvent(const std::string& Event, bool local, std::weak_ptr<TLuaFile> Caller, std::shared_ptr<TLuaArgs> arg, bool Wait) {
    std::any R;
    std::string Type;
    int Ret = 0;
    // TODO: lock
    for (auto& Script : mLuaFiles) {
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

std::shared_ptr<TLuaFile> TLuaEngine::GetLuaFileOfScript(lua_State* L) {
    std::unique_lock Lock(mLuaFilesMutex);
    for (auto& Script : mLuaFiles) {
        if (Script->GetState() == L)
            return Script;
    }
    return nullptr;
}

void TLuaEngine::FolderList(const std::string& Path, bool HotSwap) {
    for (const auto& entry : fs::directory_iterator(Path)) {
        if (fs::is_directory(entry)) {
            RegisterFiles(entry.path(), HotSwap);
        }
    }
}

void TLuaEngine::RegisterFiles(const fs::path& Path, bool HotSwap) {
    std::string Name = Path.filename();
    if (!HotSwap) {
        info(("Loading plugin : ") + Name);
    }
    for (const auto& entry : fs::directory_iterator(Path)) {
        if (entry.is_regular_file() && entry.path().extension() == ".lua") {
            if (!HotSwap || IsNewFile(entry.path().string())) {
                InsertNewLuaFile(entry.path(), Path.filename());
                if (HotSwap) {
                    info(("[HOTSWAP] Added : ") + entry.path().filename().string());
                }
            }
        }
    }
}

bool TLuaEngine::IsNewFile(const std::string& Path) {
    std::unique_lock Lock(mLuaFilesMutex);
    for (auto& Script : mLuaFiles) {
        // TODO: use std::any_of
        if (Path == Script->GetFileName())
            return false;
    }
    return true;
}

void TLuaEngine::SendError(lua_State* L, const std::string& msg) {
    Assert(L);
    auto MaybeS = GetLuaFileOfScript(L);
    std::string a;
    if (!MaybeS) {
        a = ("_Console");
    } else {
        // FIXME: GetFileName already returns the filename. This is redundant.
        a = fs::path(MaybeS->GetFileName()).filename().string();
    }
    warn(a + (" | Incorrect call of ") + msg);
}

// //////////////////////////////////////////////
// ////////// LUA *ONLY* BELOW //////////////////
// //////////////////////////////////////////////

std::shared_ptr<TClient> GetClient(TServer& Server, int ID) {
    std::shared_ptr<TClient> MaybeClient = nullptr;
    Server.ForEachClient([&](std::weak_ptr<TClient> CPtr) -> bool {
        ReadLock Lock(Server.GetClientMutex());
        if (!CPtr.expired()) {
            auto C = CPtr.lock();
            if (C->GetID() == ID) {
                MaybeClient = C;
                return false;
            }
        }
        return true;
    });
    return MaybeClient;
}

int ServerLua_GetIdentifiers(lua_State* L) {
    Assert(TheLuaEngine);
    Assert(L);
    if (lua_isnumber(L, 1)) {
        // do not store this
        // FIXME: should this not be lua_tointeger?!
        auto MaybeClient = GetClient(TheLuaEngine->Server(), int(lua_tonumber(L, 1)));
        if (MaybeClient) {
            auto IDs = MaybeClient->GetIdentifiers();
            if (IDs.empty())
                return 0;
            // FIXME: What does this entire thing do? Add comments.
            lua_newtable(L);
            for (const std::string& ID : IDs) {
                lua_pushstring(L, ID.substr(0, ID.find(':')).c_str());
                lua_pushstring(L, ID.c_str());
                lua_settable(L, -3);
            }
        } else
            return 0;
    } else {
        SendError(*TheLuaEngine, L, "lua_GetIdentifiers wrong arguments");
        return 0;
    }
    return 1;
}



// LEAVE THIS AT THE **VERY** BOTTOM OF THE FILE

std::shared_ptr<TLuaFile> TLuaEngine::InsertNewLuaFile(const fs::path& FileName, const std::string& PluginName) {
    std::shared_ptr<TLuaFile> Script(new TLuaFile(*this));
    mLuaFiles.push_back(Script);
    Script->SetPluginName(PluginName);
    Script->SetFileName(FileName);
    Script->SetLastWrite(fs::last_write_time(FileName));
    lua_State* State = Script->GetState();
    luaL_openlibs(State);
    lua_register(State, "GetPlayerIdentifiers", ServerLua_GetIdentifiers);
    lua_register(State, "TriggerGlobalEvent", lua_TriggerEventG);
    lua_register(State, "TriggerLocalEvent", lua_TriggerEventL);
    lua_register(State, "TriggerClientEvent", lua_RemoteEvent);
    lua_register(State, "GetPlayerCount", lua_GetPlayerCount);
    lua_register(State, "isPlayerConnected", lua_isConnected);
    lua_register(State, "RegisterEvent", lua_RegisterEvent);
    lua_register(State, "GetPlayerName", lua_GetPlayerName);
    lua_register(State, "RemoveVehicle", lua_RemoveVehicle);
    lua_register(State, "GetPlayerDiscordID", lua_TempFix);
    lua_register(State, "CreateThread", lua_CreateThread);
    lua_register(State, "GetPlayerVehicles", lua_GetCars);
    lua_register(State, "SendChatMessage", lua_sendChat);
    lua_register(State, "GetPlayers", lua_GetAllPlayers);
    lua_register(State, "GetPlayerGuest", lua_GetGuest);
    lua_register(State, "StopThread", lua_StopThread);
    lua_register(State, "DropPlayer", lua_dropPlayer);
    lua_register(State, "GetPlayerHWID", lua_HWID);
    lua_register(State, "exit", lua_ServerExit);
    lua_register(State, "Sleep", lua_Sleep);
    lua_register(State, "print", lua_Print);
    lua_register(State, "Set", lua_Set);
    if (!Script->IsConsole()) {
        Reload();
    }
    debug("inserted new lua file: " + PluginName + " - " + FileName.string());
    return Script;
}
