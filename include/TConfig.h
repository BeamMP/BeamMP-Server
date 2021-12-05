#pragma once

#include "Common.h"

#include <atomic>

namespace fs = std::filesystem;

class TConfig {
public:
    explicit TConfig(const std::string& ConfigFileName);

    [[nodiscard]] bool Failed() const { return mFailed; }

    void FlushToFile();

private:
    void CreateConfigFile(std::string_view name);
    void ParseFromFile(std::string_view name);
    void PrintDebug();

    void ParseOldFormat();
    bool IsDefault();
    bool mFailed { false };
    std::string mConfigFileName;
};

