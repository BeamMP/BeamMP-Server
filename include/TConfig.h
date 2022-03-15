#pragma once

#include "Common.h"

#include <atomic>
#include <filesystem>

#define TOML11_PRESERVE_COMMENTS_BY_DEFAULT
#include <toml11/toml.hpp> // header-only version of TOML++

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
    void TryReadValue(toml::value& Table, const std::string& Category, const std::string_view& Key, std::string& OutValue);
    void TryReadValue(toml::value& Table, const std::string& Category, const std::string_view& Key, bool& OutValue);
    void TryReadValue(toml::value& Table, const std::string& Category, const std::string_view& Key, int& OutValue);

    void ParseOldFormat();
    bool IsDefault();
    bool mFailed { false };
    std::string mConfigFileName;
};
