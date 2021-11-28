#pragma once

#include "Cryptography.h"
#include "commandline.h"
#include <atomic>
#include <fstream>

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
    void ChangeToLuaConsole();
    void ChangeToRegularConsole();

    Commandline mCommandline;
    std::vector<std::string> mCachedLuaHistory;
    std::vector<std::string> mCachedRegularHistory;
    TLuaEngine* mLuaEngine { nullptr };
    bool mIsLuaConsole { false };
    bool mFirstTime { true };
    const std::string mStateId = "BEAMMP_SERVER_CONSOLE";
};
