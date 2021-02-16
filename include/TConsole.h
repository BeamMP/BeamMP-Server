#pragma once

#include "commandline/commandline.h"
#include <atomic>
#include <fstream>

class TConsole {
public:
    TConsole();

    void Write(const std::string& str);

private:
    Commandline mCommandline;
};
