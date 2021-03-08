#pragma once

#include "commandline/commandline.h"
#include "TLuaFile.h"
#include <atomic>
#include <fstream>

class TConsole {
public:
    TConsole();

    void Write(const std::string& str);
    void WriteRaw(const std::string& str);
    void InitializeLuaConsole(TLuaEngine& Engine);

private:
    std::unique_ptr<TLuaFile> mLuaConsole { nullptr };
    Commandline mCommandline;
};
