#include <toml.hpp> // header-only version of TOML++

#include "TConfig.h"
#include <fstream>
#include <iostream>

static const char* ConfigFileName = static_cast<const char*>("ServerConfig.toml");

static constexpr std::string_view StrDebug = "Debug";
static constexpr std::string_view StrPrivate = "Private";
static constexpr std::string_view StrPort = "Port";
static constexpr std::string_view StrMaxCars = "MaxCars";
static constexpr std::string_view StrMaxPlayers = "MaxPlayers";
static constexpr std::string_view StrMap = "Map";
static constexpr std::string_view StrName = "Name";
static constexpr std::string_view StrDescription = "Description";
static constexpr std::string_view StrResourceFolder = "ResourceFolder";
static constexpr std::string_view StrAuthKey = "AuthKey";

TConfig::TConfig() {
    if (!fs::exists(ConfigFileName) || !fs::is_regular_file(ConfigFileName)) {
        info("No config file found! Generating one...");
        CreateConfigFile(ConfigFileName);
    }
    if (!mFailed) {
        if (fs::exists("Server.cfg")) {
            warn("An old \"Server.cfg\" file still exists. Please note that this is no longer used. Instead, \"" + std::string(ConfigFileName) + "\" is used. You can safely delete the \"Server.cfg\".");
        }
        ParseFromFile(ConfigFileName);
    }
}

void TConfig::CreateConfigFile(std::string_view name) {
    toml::table tbl { {

        { "General",
            toml::table { {

                { StrDebug, Application::Settings.DebugModeEnabled },
                { StrPrivate, Application::Settings.Private },
                { StrPort, Application::Settings.Port },
                { StrMaxCars, Application::Settings.MaxCars },
                { StrMaxPlayers, Application::Settings.MaxPlayers },
                { StrMap, Application::Settings.MapName },
                { StrName, Application::Settings.ServerName },
                { StrDescription, Application::Settings.ServerDesc },
                { StrResourceFolder, Application::Settings.Resource },
                { StrAuthKey, Application::Settings.Key },

            } } },

    } };
    std::ofstream ofs { std::string(name) };
    if (ofs.good()) {
        ofs << "# This is the BeamMP-Server config file.\n"
               "# Help & Documentation: `https://wiki.beammp.com/en/home/server-maintenance`\n"
               "# IMPORTANT: Fill in the AuthKey with the key you got from `https://beammp.com/k/dashboard` on the left under \"Keys\"\n"
            << '\n';
        ofs << tbl << '\n';
        error("There was no \"" + std::string(ConfigFileName) + "\" file (this is normal for the first time running the server), so one was generated for you. Please open it and fill in your AuthKey and other settings, then restart the server. The old Server.cfg file will no longer be used and cause a warning if it exists from now on.");
        mFailed = true;
    } else {
        error("Couldn't create " + std::string(name) + ". Check permissions, try again, and contact support if it continues not to work.");
        mFailed = true;
    }
}

void TConfig::ParseFromFile(std::string_view name) {
    try {
        toml::table FullTable = toml::parse_file(name);
        toml::table GeneralTable = *FullTable["General"].as_table();
        if (auto val = GeneralTable[StrDebug].value<bool>(); val.has_value()) {
            Application::Settings.DebugModeEnabled = val.value();
        } else {
            throw std::runtime_error(std::string(StrDebug));
        }
        if (auto val = GeneralTable[StrPrivate].value<bool>(); val.has_value()) {
            Application::Settings.Private = val.value();
        } else {
            throw std::runtime_error(std::string(StrPrivate));
        }
        if (auto val = GeneralTable[StrPort].value<int>(); val.has_value()) {
            Application::Settings.Port = val.value();
        } else {
            throw std::runtime_error(std::string(StrPort));
        }
        if (auto val = GeneralTable[StrMaxCars].value<int>(); val.has_value()) {
            Application::Settings.MaxCars = val.value();
        } else {
            throw std::runtime_error(std::string(StrMaxCars));
        }
        if (auto val = GeneralTable[StrMaxPlayers].value<int>(); val.has_value()) {
            Application::Settings.MaxPlayers = val.value();
        } else {
            throw std::runtime_error(std::string(StrMaxPlayers));
        }
        if (auto val = GeneralTable[StrMap].value<std::string>(); val.has_value()) {
            Application::Settings.MapName = val.value();
        } else {
            throw std::runtime_error(std::string(StrMap));
        }
        if (auto val = GeneralTable[StrName].value<std::string>(); val.has_value()) {
            Application::Settings.ServerName = val.value();
        } else {
            throw std::runtime_error(std::string(StrName));
        }
        if (auto val = GeneralTable[StrDescription].value<std::string>(); val.has_value()) {
            Application::Settings.ServerDesc = val.value();
        } else {
            throw std::runtime_error(std::string(StrDescription));
        }
        if (auto val = GeneralTable[StrResourceFolder].value<std::string>(); val.has_value()) {
            Application::Settings.Resource = val.value();
        } else {
            throw std::runtime_error(std::string(StrResourceFolder));
        }
        if (auto val = GeneralTable[StrAuthKey].value<std::string>(); val.has_value()) {
            Application::Settings.Key = val.value();
        } else {
            throw std::runtime_error(std::string(StrAuthKey));
        }
    } catch (const std::exception& err) {
        error("Error parsing config file value: " + std::string(err.what()));
        mFailed = true;
        return;
    }
    PrintDebug();
    // all good so far, let's check if there's a key
    if (Application::Settings.Key.empty()) {
        error("No AuthKey specified in the \"" + std::string(ConfigFileName) + "\" file. Please get an AuthKey, enter it into the config file, and restart this server.");
        mFailed = true;
    }
}

void TConfig::PrintDebug() {
    debug(std::string(StrDebug) + ": " + std::string(Application::Settings.DebugModeEnabled ? "true" : "false"));
    debug(std::string(StrPrivate) + ": " + std::string(Application::Settings.Private ? "true" : "false"));
    debug(std::string(StrPort) + ": " + std::to_string(Application::Settings.Port));
    debug(std::string(StrMaxCars) + ": " + std::to_string(Application::Settings.MaxCars));
    debug(std::string(StrMaxPlayers) + ": " + std::to_string(Application::Settings.MaxPlayers));
    debug(std::string(StrMap) + ": \"" + Application::Settings.MapName + "\"");
    debug(std::string(StrName) + ": \"" + Application::Settings.ServerName + "\"");
    debug(std::string(StrDescription) + ": \"" + Application::Settings.ServerDesc + "\"");
    debug(std::string(StrResourceFolder) + ": \"" + Application::Settings.Resource + "\"");
    // special!
    debug("Key Length: " + std::to_string(Application::Settings.Key.length()) + "");
}
