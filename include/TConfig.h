#pragma once

#include "Common.h"

#include <atomic>

class TConfig {
public:
    explicit TConfig();

    bool Failed() const { return mFailed; }

private:
    void CreateConfigFile(std::string_view name);
    void ParseFromFile(std::string_view name);
    void PrintDebug();

    bool mFailed { false };
};
