#pragma once

#include <atomic>
#include <commandline/commandline.h>

class TConsole {
public:
    TConsole();

private:
    Commandline _Commandline;
};
