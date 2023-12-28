#pragma once

#include "Common.h"

#include <atomic>
#include <filesystem>

#define TOML11_PRESERVE_COMMENTS_BY_DEFAULT
#include <toml.hpp> // header-only version of TOML++

namespace fs = std::filesystem;

class TConfig {
public:
    explicit TConfig(const std::string& ConfigFileName);

    [[nodiscard]] bool Failed() const { return mFailed; }

    void FlushToFile();

private:
    void CreateConfigFile();
    void ParseFromFile(std::string_view name);
    void PrintDebug();
    void TryReadValue(toml::value& Table, const std::string& Category, const std::string_view& Key, const std::string_view& Env, std::string& OutValue);
    void TryReadValue(toml::value& Table, const std::string& Category, const std::string_view& Key, const std::string_view& Env, bool& OutValue);
    void TryReadValue(toml::value& Table, const std::string& Category, const std::string_view& Key, const std::string_view& Env, int& OutValue);

    void ParseOldFormat();
    std::string TagsAsPrettyArray() const;
    bool IsDefault();
    bool mFailed { false };
    std::string mConfigFileName;
};
