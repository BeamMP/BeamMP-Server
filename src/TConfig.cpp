// BeamMP, the BeamNG.drive multiplayer mod.
// Copyright (C) 2024 BeamMP Ltd., BeamMP team and contributors.
//
// BeamMP Ltd. can be contacted by electronic mail via contact@beammp.com.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "Common.h"

#include "Env.h"
#include "TConfig.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <istream>
#include <sstream>

// General
static constexpr std::string_view StrDebug = "Debug";
static constexpr std::string_view EnvStrDebug = "BEAMMP_DEBUG";
static constexpr std::string_view StrPrivate = "Private";
static constexpr std::string_view EnvStrPrivate = "BEAMMP_PRIVATE";
static constexpr std::string_view StrPort = "Port";
static constexpr std::string_view EnvStrPort = "BEAMMP_PORT";
static constexpr std::string_view StrMaxCars = "MaxCars";
static constexpr std::string_view EnvStrMaxCars = "BEAMMP_MAX_CARS";
static constexpr std::string_view StrMaxPlayers = "MaxPlayers";
static constexpr std::string_view EnvStrMaxPlayers = "BEAMMP_MAX_PLAYERS";
static constexpr std::string_view StrMap = "Map";
static constexpr std::string_view EnvStrMap = "BEAMMP_MAP";
static constexpr std::string_view StrName = "Name";
static constexpr std::string_view EnvStrName = "BEAMMP_NAME";
static constexpr std::string_view StrDescription = "Description";
static constexpr std::string_view EnvStrDescription = "BEAMMP_DESCRIPTION";
static constexpr std::string_view StrTags = "Tags";
static constexpr std::string_view EnvStrTags = "BEAMMP_TAGS";
static constexpr std::string_view StrResourceFolder = "ResourceFolder";
static constexpr std::string_view EnvStrResourceFolder = "BEAMMP_RESOURCE_FOLDER";
static constexpr std::string_view StrAuthKey = "AuthKey";
static constexpr std::string_view EnvStrAuthKey = "BEAMMP_AUTH_KEY";
static constexpr std::string_view StrLogChat = "LogChat";
static constexpr std::string_view EnvStrLogChat = "BEAMMP_LOG_CHAT";
static constexpr std::string_view StrPassword = "Password";

// Misc
static constexpr std::string_view StrSendErrors = "SendErrors";
static constexpr std::string_view StrSendErrorsMessageEnabled = "SendErrorsShowMessage";
static constexpr std::string_view StrHideUpdateMessages = "ImScaredOfUpdates";

TEST_CASE("TConfig::TConfig") {
    const std::string CfgFile = "beammp_server_testconfig.toml";
    fs::remove(CfgFile);

    TConfig Cfg(CfgFile);

    CHECK(fs::file_size(CfgFile) != 0);

    std::string buf;
    {
        buf.resize(fs::file_size(CfgFile));
        auto fp = std::fopen(CfgFile.c_str(), "r");
        auto res = std::fread(buf.data(), 1, buf.size(), fp);
        if (res != buf.size()) {
            // IGNORE?
        }
        std::fclose(fp);
    }
    INFO("file contents are:", buf);

    const auto table = toml::parse(CfgFile);
    CHECK(table.at("General").is_table());
    CHECK(table.at("Misc").is_table());

    fs::remove(CfgFile);
}

TConfig::TConfig(const std::string& ConfigFileName)
    : mConfigFileName(ConfigFileName) {
    Application::SetSubsystemStatus("Config", Application::Status::Starting);
    auto DisableConfig = Env::Get(Env::Key::PROVIDER_DISABLE_CONFIG).value_or("false");
    mDisableConfig = DisableConfig == "true" || DisableConfig == "1";
    if (!mDisableConfig && (!fs::exists(mConfigFileName) || !fs::is_regular_file(mConfigFileName))) {
        beammp_info("No config file found! Generating one...");
        CreateConfigFile();
    }
    if (!mFailed) {
        if (fs::exists("Server.cfg")) {
            beammp_warn("An old \"Server.cfg\" file still exists. Please note that this is no longer used. Instead, \"" + std::string(mConfigFileName) + "\" is used. You can safely delete the \"Server.cfg\".");
        }
        ParseFromFile(mConfigFileName);
    }
}

template <typename CommentsT>
void SetComment(CommentsT& Comments, const std::string& Comment) {
    Comments.clear();
    Comments.push_back(Comment);
}

/**
 * @brief Writes out the loaded application state into ServerConfig.toml
 *
 * This writes out the current state of application settings that are
 * applied to the server instance (i.e. the current application settings loaded in the server).
 * If the state of the application settings changes during runtime,
 * call this function whenever something about the config changes
 * whether it is in TConfig.cpp or the configuration file.
 */
void TConfig::FlushToFile() {
    // auto data = toml::parse<toml::preserve_comments>(mConfigFileName);
    auto data = toml::value {};
    data["General"][StrAuthKey.data()] = Application::Settings.Key;
    SetComment(data["General"][StrAuthKey.data()].comments(), " AuthKey has to be filled out in order to run the server");
    data["General"][StrLogChat.data()] = Application::Settings.LogChat;
    SetComment(data["General"][StrLogChat.data()].comments(), " Whether to log chat messages in the console / log");
    data["General"][StrDebug.data()] = Application::Settings.DebugModeEnabled;
    data["General"][StrPrivate.data()] = Application::Settings.Private;
    data["General"][StrPort.data()] = Application::Settings.Port;
    data["General"][StrName.data()] = Application::Settings.ServerName;
    SetComment(data["General"][StrTags.data()].comments(), " Add custom identifying tags to your server to make it easier to find. Format should be TagA,TagB,TagC. Note the comma seperation.");
    data["General"][StrTags.data()] = Application::Settings.ServerTags;
    data["General"][StrMaxCars.data()] = Application::Settings.MaxCars;
    data["General"][StrMaxPlayers.data()] = Application::Settings.MaxPlayers;
    data["General"][StrMap.data()] = Application::Settings.MapName;
    data["General"][StrDescription.data()] = Application::Settings.ServerDesc;
    data["General"][StrResourceFolder.data()] = Application::Settings.Resource;
    // data["General"][StrPassword.data()] = Application::Settings.Password;
    // SetComment(data["General"][StrPassword.data()].comments(), " Sets a password on this server, which restricts people from joining. To join, a player must enter this exact password. Leave empty ("") to disable the password.");
    // Misc
    data["Misc"][StrHideUpdateMessages.data()] = Application::Settings.HideUpdateMessages;
    SetComment(data["Misc"][StrHideUpdateMessages.data()].comments(), " Hides the periodic update message which notifies you of a new server version. You should really keep this on and always update as soon as possible. For more information visit https://wiki.beammp.com/en/home/server-maintenance#updating-the-server. An update message will always appear at startup regardless.");
    data["Misc"][StrSendErrors.data()] = Application::Settings.SendErrors;
    SetComment(data["Misc"][StrSendErrors.data()].comments(), " If SendErrors is `true`, the server will send helpful info about crashes and other issues back to the BeamMP developers. This info may include your config, who is on your server at the time of the error, and similar general information. This kind of data is vital in helping us diagnose and fix issues faster. This has no impact on server performance. You can opt-out of this system by setting this to `false`");
    data["Misc"][StrSendErrorsMessageEnabled.data()] = Application::Settings.SendErrorsMessageEnabled;
    SetComment(data["Misc"][StrSendErrorsMessageEnabled.data()].comments(), " You can turn on/off the SendErrors message you get on startup here");
    std::stringstream Ss;
    Ss << "# This is the BeamMP-Server config file.\n"
          "# Help & Documentation: `https://docs.beammp.com/server/server-maintenance/`\n"
          "# IMPORTANT: Fill in the AuthKey with the key you got from `https://keymaster.beammp.com/` on the left under \"Keys\"\n"
       << data;
    auto File = std::fopen(mConfigFileName.c_str(), "w+");
    if (!File) {
        beammp_error("Failed to create/write to config file: " + GetPlatformAgnosticErrorString());
        throw std::runtime_error("Failed to create/write to config file");
    }
    auto Str = Ss.str();
    auto N = std::fwrite(Str.data(), sizeof(char), Str.size(), File);
    if (N != Str.size()) {
        beammp_error("Failed to write to config file properly, config file might be misshapen");
    }
    std::fclose(File);
}

void TConfig::CreateConfigFile() {
    // build from old config Server.cfg
    if (mDisableConfig) {
        return;
    }
    try {
        if (fs::exists("Server.cfg")) {
            // parse it (this is weird and bad and should be removed in some future version)
            ParseOldFormat();
        }
    } catch (const std::exception& e) {
        beammp_error("an error occurred and was ignored during config transfer: " + std::string(e.what()));
    }

    FlushToFile();
}

void TConfig::TryReadValue(toml::value& Table, const std::string& Category, const std::string_view& Key, const std::string_view& Env, std::string& OutValue) {
    if (!Env.empty()) {
        // If this environment variable exists, return a C-String and check if it's empty or not
        if (const char* envp = std::getenv(Env.data()); envp != nullptr && std::strcmp(envp, "") != 0) {
            OutValue = std::string(envp);
            return;
        }
    }
    if (mDisableConfig) {
        return;
    }
    if (Table[Category.c_str()][Key.data()].is_string()) {
        OutValue = Table[Category.c_str()][Key.data()].as_string();
    }
}

void TConfig::TryReadValue(toml::value& Table, const std::string& Category, const std::string_view& Key, const std::string_view& Env, bool& OutValue) {
    if (!Env.empty()) {
        // If this environment variable exists, return a C-String and check if it's empty or not
        if (const char* envp = std::getenv(Env.data()); envp != nullptr && std::strcmp(envp, "") != 0) {
            auto Str = std::string(envp);
            OutValue = Str == "1" || Str == "true";
            return;
        }
    }
    if (mDisableConfig) {
        return;
    }
    if (Table[Category.c_str()][Key.data()].is_boolean()) {
        OutValue = Table[Category.c_str()][Key.data()].as_boolean();
    }
}

void TConfig::TryReadValue(toml::value& Table, const std::string& Category, const std::string_view& Key, const std::string_view& Env, int& OutValue) {
    if (!Env.empty()) {
        // If this environment variable exists, return a C-String and check if it's empty or not
        if (const char* envp = std::getenv(Env.data()); envp != nullptr && std::strcmp(envp, "") != 0) {
            OutValue = int(std::strtol(envp, nullptr, 10));
            return;
        }
    }
    if (mDisableConfig) {
        return;
    }
    if (Table[Category.c_str()][Key.data()].is_integer()) {
        OutValue = int(Table[Category.c_str()][Key.data()].as_integer());
    }
}

// This arcane template magic is needed for using lambdas as overloaded visitors
// See https://en.cppreference.com/w/cpp/utility/variant/visit for reference
template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

void TConfig::TryReadValue(toml::value& Table, const std::string& Category, const std::string_view& Key, const std::string_view& Env, Settings::Key key) {
    if (!Env.empty()) {
        if (const char* envp = std::getenv(Env.data()); envp != nullptr && std::strcmp(envp, "") != 0) {

            std::visit(
                overloaded {
                    [&envp, &key](std::string) {
                        Application::SettingsSingleton.set(key, std::string(envp));
                    },
                    [&envp, &key](int) {
                        Application::SettingsSingleton.set(key, int(std::strtol(envp, nullptr, 10)));
                    },
                    [&envp, &key](bool) {
                        auto Str = std::string(envp);
                        Application::SettingsSingleton.set(key, bool(Str == "1" || Str == "true"));
                    } },
                Application::SettingsSingleton.get(key));
        }
    } else {

        std::visit(
            overloaded {
                [&Table, &Category, &Key, &key](std::string) {
                    if (Table[Category.c_str()][Key.data()].is_string())
                        Application::SettingsSingleton.set(key, Table[Category.c_str()][Key.data()].as_string());
                    else
                        beammp_warnf("Value '{}.{}' has unexpected type, expected type 'string'", Category, Key);
                },
                [&Table, &Category, &Key, &key](int) {
                    if (Table[Category.c_str()][Key.data()].is_integer())
                        Application::SettingsSingleton.set(key, int(Table[Category.c_str()][Key.data()].as_integer()));
                    else
                        beammp_warnf("Value '{}.{}' has unexpected type, expected type 'integer'", Category, Key);
                },
                [&Table, &Category, &Key, &key](bool) {
                    if (Table[Category.c_str()][Key.data()].is_boolean())
                        Application::SettingsSingleton.set(key, Table[Category.c_str()][Key.data()].as_boolean());
                    else
                        beammp_warnf("Value '{}.{}' has unexpected type, expected type 'boolean'", Category, Key);
                } },
            Application::SettingsSingleton.get(key));
    }
}

void TConfig::ParseFromFile(std::string_view name) {
    try {
        toml::value data {};
        if (!mDisableConfig) {
            data = toml::parse<toml::preserve_comments>(name.data());
        }
        // GENERAL
        TryReadValue(data, "General", StrDebug, EnvStrDebug, Application::Settings.DebugModeEnabled);
        TryReadValue(data, "General", StrPrivate, EnvStrPrivate, Application::Settings.Private);
        if (Env::Get(Env::Key::PROVIDER_PORT_ENV).has_value()) {
            TryReadValue(data, "General", StrPort, Env::Get(Env::Key::PROVIDER_PORT_ENV).value(), Application::Settings.Port);
        } else {
            TryReadValue(data, "General", StrPort, EnvStrPort, Application::Settings.Port);
        }
        TryReadValue(data, "General", StrMaxCars, EnvStrMaxCars, Application::Settings.MaxCars);
        TryReadValue(data, "General", StrMaxPlayers, EnvStrMaxPlayers, Application::Settings.MaxPlayers);
        TryReadValue(data, "General", StrMap, EnvStrMap, Application::Settings.MapName);
        TryReadValue(data, "General", StrName, EnvStrName, Application::Settings.ServerName);
        TryReadValue(data, "General", StrDescription, EnvStrDescription, Application::Settings.ServerDesc);
        TryReadValue(data, "General", StrTags, EnvStrTags, Application::Settings.ServerTags);
        TryReadValue(data, "General", StrResourceFolder, EnvStrResourceFolder, Application::Settings.Resource);
        TryReadValue(data, "General", StrAuthKey, EnvStrAuthKey, Application::Settings.Key);
        TryReadValue(data, "General", StrLogChat, EnvStrLogChat, Application::Settings.LogChat);
        TryReadValue(data, "General", StrPassword, "", Application::Settings.Password);
        // Misc
        TryReadValue(data, "Misc", StrSendErrors, "", Application::Settings.SendErrors);
        TryReadValue(data, "Misc", StrHideUpdateMessages, "", Application::Settings.HideUpdateMessages);
        TryReadValue(data, "Misc", StrSendErrorsMessageEnabled, "", Application::Settings.SendErrorsMessageEnabled);

        // Read into new Settings Singleton
        TryReadValue(data, "General", StrDebug, EnvStrDebug, Settings::Key::General_Debug);
        TryReadValue(data, "General", StrPrivate, EnvStrPrivate, Settings::Key::General_Private);
        TryReadValue(data, "General", StrPort, EnvStrPort, Settings::Key::General_Port);
        TryReadValue(data, "General", StrMaxCars, EnvStrMaxCars, Settings::Key::General_MaxCars);
        TryReadValue(data, "General", StrMaxPlayers, EnvStrMaxPlayers, Settings::Key::General_MaxPlayers);
        TryReadValue(data, "General", StrMap, EnvStrMap, Settings::Key::General_Map);
        TryReadValue(data, "General", StrName, EnvStrName, Settings::Key::General_Name);
        TryReadValue(data, "General", StrDescription, EnvStrDescription, Settings::Key::General_Description);
        TryReadValue(data, "General", StrTags, EnvStrTags, Settings::Key::General_Tags);
        TryReadValue(data, "General", StrResourceFolder, EnvStrResourceFolder, Settings::Key::General_ResourceFolder);
        TryReadValue(data, "General", StrAuthKey, EnvStrAuthKey, Settings::Key::General_AuthKey);
        TryReadValue(data, "General", StrLogChat, EnvStrLogChat, Settings::Key::General_LogChat);
        // Misc
        TryReadValue(data, "Misc", StrSendErrors, "", Settings::Key::Misc_SendErrors);
        TryReadValue(data, "Misc", StrHideUpdateMessages, "", Settings::Misc_ImScaredOfUpdates);
        TryReadValue(data, "Misc", StrSendErrorsMessageEnabled, "", Settings::Misc_SendErrorsShowMessage);

    } catch (const std::exception& err) {
        beammp_error("Error parsing config file value: " + std::string(err.what()));
        mFailed = true;
        Application::SetSubsystemStatus("Config", Application::Status::Bad);
        return;
    }

    // Update in any case
    if (!mDisableConfig) {
        FlushToFile();
    }
    // all good so far, let's check if there's a key
    if (Application::Settings.Key.empty()) {
        if (mDisableConfig) {
            beammp_error("No AuthKey specified in the environment.");
        } else {
            beammp_error("No AuthKey specified in the \"" + std::string(mConfigFileName) + "\" file. Please get an AuthKey, enter it into the config file, and restart this server.");
        }
        Application::SetSubsystemStatus("Config", Application::Status::Bad);
        mFailed = true;
        return;
    }
    Application::SetSubsystemStatus("Config", Application::Status::Good);
    if (Application::Settings.Key.size() != 36) {
        beammp_warn("AuthKey specified is the wrong length and likely isn't valid.");
    }
}

void TConfig::PrintDebug() {
    if (mDisableConfig) {
        beammp_debug("Provider turned off the generation and parsing of the ServerConfig.toml");
    }
    beammp_debug(std::string(StrDebug) + ": " + std::string(Application::Settings.DebugModeEnabled ? "true" : "false"));
    beammp_debug(std::string(StrPrivate) + ": " + std::string(Application::Settings.Private ? "true" : "false"));
    beammp_debug(std::string(StrPort) + ": " + std::to_string(Application::Settings.Port));
    beammp_debug(std::string(StrMaxCars) + ": " + std::to_string(Application::Settings.MaxCars));
    beammp_debug(std::string(StrMaxPlayers) + ": " + std::to_string(Application::Settings.MaxPlayers));
    beammp_debug(std::string(StrMap) + ": \"" + Application::Settings.MapName + "\"");
    beammp_debug(std::string(StrName) + ": \"" + Application::Settings.ServerName + "\"");
    beammp_debug(std::string(StrDescription) + ": \"" + Application::Settings.ServerDesc + "\"");
    beammp_debug(std::string(StrTags) + ": " + TagsAsPrettyArray());
    beammp_debug(std::string(StrLogChat) + ": \"" + (Application::Settings.LogChat ? "true" : "false") + "\"");
    beammp_debug(std::string(StrResourceFolder) + ": \"" + Application::Settings.Resource + "\"");
    // special!
    beammp_debug("Key Length: " + std::to_string(Application::Settings.Key.length()) + "");
    beammp_debug("Password Protected: " + std::string(Application::Settings.Password.empty() ? "false" : "true"));
}

void TConfig::ParseOldFormat() {
    std::ifstream File("Server.cfg");
    // read all, strip comments
    std::string Content;
    for (;;) {
        std::string Line;
        std::getline(File, Line);
        if (!Line.empty() && Line.at(0) != '#') {
            Line = Line.substr(0, Line.find_first_of('#'));
            Content += Line + "\n";
        }
        if (!File.good()) {
            break;
        }
    }
    std::stringstream Str(Content);
    std::string Key, Ignore, Value;
    for (;;) {
        Str >> Key >> std::ws >> Ignore >> std::ws;
        std::getline(Str, Value);
        if (Str.eof()) {
            break;
        }
        std::stringstream ValueStream(Value);
        ValueStream >> std::ws; // strip leading whitespace if any
        Value = ValueStream.str();
        if (Key == "Debug") {
            Application::Settings.DebugModeEnabled = Value.find("true") != std::string::npos;
        } else if (Key == "Private") {
            Application::Settings.Private = Value.find("true") != std::string::npos;
        } else if (Key == "Port") {
            ValueStream >> Application::Settings.Port;
        } else if (Key == "Cars") {
            ValueStream >> Application::Settings.MaxCars;
        } else if (Key == "MaxPlayers") {
            ValueStream >> Application::Settings.MaxPlayers;
        } else if (Key == "Map") {
            Application::Settings.MapName = Value.substr(1, Value.size() - 3);
        } else if (Key == "Name") {
            Application::Settings.ServerName = Value.substr(1, Value.size() - 3);
        } else if (Key == "Desc") {
            Application::Settings.ServerDesc = Value.substr(1, Value.size() - 3);
        } else if (Key == "use") {
            Application::Settings.Resource = Value.substr(1, Value.size() - 3);
        } else if (Key == "AuthKey") {
            Application::Settings.Key = Value.substr(1, Value.size() - 3);
        } else {
            beammp_warn("unknown key in old auth file (ignored): " + Key);
        }
        Str >> std::ws;
    }
}
std::string TConfig::TagsAsPrettyArray() const {
    std::vector<std::string> TagsArray = {};
    SplitString(Application::Settings.ServerTags, ',', TagsArray);
    std::string Pretty = {};
    for (size_t i = 0; i < TagsArray.size() - 1; ++i) {
        Pretty += '\"' + TagsArray[i] + "\", ";
    }
    Pretty += '\"' + TagsArray.at(TagsArray.size() - 1) + "\"";
    return Pretty;
}
