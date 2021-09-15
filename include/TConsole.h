#pragma once

#include <atomic>
#include <fstream>
#include "commandline.h"

class TConsole {
public:
    TConsole();

    void Write(const std::string& str);
    void WriteRaw(const std::string& str);
   // BROKEN void InitializeLuaConsole(TLuaEngine& Engine);

private:
// BROKEN    std::unique_ptr<TLuaFile> mLuaConsole { nullptr };
    Commandline mCommandline;
};
