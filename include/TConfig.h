#pragma once

#include "Common.h"

class TConfig {
public:
    TConfig(const std::string& ConfigFile);

private:
    std::string RemoveComments(const std::string& Line);
    void SetValues(const std::string& Line, int Index);
};
