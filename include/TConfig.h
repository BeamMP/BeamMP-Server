#pragma once

#include "Common.h"

class TConfig {
public:
    TConfig(const std::string& ConfigFile);

private:
    static std::string RemoveComments(const std::string& Line);
    static void SetValues(const std::string& Line, int Index);
};
