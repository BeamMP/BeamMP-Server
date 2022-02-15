#pragma once

#include "Cryptography.h"
#include "commandline.h"
#include <atomic>
#include <fstream>
#include <string>
#include <tuple>
#include <vector>

class TLuaEngine;

class TConsole {
public:
    TConsole();

    void Write(const std::string& str);
    void WriteRaw(const std::string& str);
    void InitializeLuaConsole(TLuaEngine& Engine);
    void BackupOldLog();
    Commandline& Internal() { return mCommandline; }

private:
    void RunAsCommand(const std::string& cmd, bool IgnoreNotACommand = false);
    void ChangeToLuaConsole(const std::string& LuaStateId);
    void ChangeToRegularConsole();
    void HandleLuaInternalCommand(const std::string& cmd);

    void Command_Lua(const std::string& cmd, const std::vector<std::string>& args);
    void Command_Help(const std::string& cmd, const std::vector<std::string>& args);
    void Command_Kick(const std::string& cmd, const std::vector<std::string>& args);
    void Command_List(const std::string& cmd, const std::vector<std::string>& args);
    void Command_Status(const std::string& cmd, const std::vector<std::string>& args);
    void Command_Settings(const std::string& cmd, const std::vector<std::string>& args);

    void Command_Say(const std::string& FullCommand);
    bool ExpectArgsCount(const std::vector<std::string>& args, size_t n);
    bool ExpectArgsCount(const std::vector<std::string>& args, size_t min, size_t max);

    static std::tuple<std::string, std::vector<std::string>> ParseCommand(const std::string& cmd);

    Commandline mCommandline;
    std::vector<std::string> mCachedLuaHistory;
    std::vector<std::string> mCachedRegularHistory;
    TLuaEngine* mLuaEngine { nullptr };
    bool mIsLuaConsole { false };
    bool mFirstTime { true };
    std::string mStateId;
    const std::string mDefaultStateId = "BEAMMP_SERVER_CONSOLE";
};
