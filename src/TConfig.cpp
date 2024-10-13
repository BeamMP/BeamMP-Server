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
#include "Settings.h"
#include "TConfig.h"
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <istream>
#include <sstream>
#include <type_traits>

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
static constexpr std::string_view StrAllowGuests = "AllowGuests";
static constexpr std::string_view EnvStrAllowGuests = "BEAMMP_ALLOW_GUESTS";
static constexpr std::string_view StrInformationPacket = "InformationPacket";
static constexpr std::string_view EnvStrInformationPacket = "BEAMMP_INFORMATION_PACKET";
static constexpr std::string_view StrPassword = "Password";

// Misc
static constexpr std::string_view StrSendErrors = "SendErrors";
static constexpr std::string_view StrSendErrorsMessageEnabled = "SendErrorsShowMessage";
static constexpr std::string_view StrHideUpdateMessages = "ImScaredOfUpdates";
static constexpr std::string_view StrUpdateReminderTime = "UpdateReminderTime";

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
    data["General"][StrAuthKey.data()] = Application::Settings.getAsString(Settings::Key::General_AuthKey);
    SetComment(data["General"][StrAuthKey.data()].comments(), " AuthKey has to be filled out in order to run the server");
    data["General"][StrLogChat.data()] = Application::Settings.getAsBool(Settings::Key::General_LogChat);
    SetComment(data["General"][StrLogChat.data()].comments(), " Whether to log chat messages in the console / log");
    data["General"][StrDebug.data()] = Application::Settings.getAsBool(Settings::Key::General_Debug);
    data["General"][StrPrivate.data()] = Application::Settings.getAsBool(Settings::Key::General_Private);
    SetComment(data["General"][StrInformationPacket.data()].comments(), " Whether to allow unconnected clients to get the public server information without joining");
    data["General"][StrInformationPacket.data()] = Application::Settings.getAsBool(Settings::Key::General_InformationPacket);
    data["General"][StrAllowGuests.data()] = Application::Settings.getAsBool(Settings::Key::General_AllowGuests);
    SetComment(data["General"][StrAllowGuests.data()].comments(), " Whether to allow guests");
    data["General"][StrPort.data()] = Application::Settings.getAsInt(Settings::Key::General_Port);
    data["General"][StrName.data()] = Application::Settings.getAsString(Settings::Key::General_Name);
    SetComment(data["General"][StrTags.data()].comments(), " Add custom identifying tags to your server to make it easier to find. Format should be TagA,TagB,TagC. Note the comma seperation.");
    data["General"][StrTags.data()] = Application::Settings.getAsString(Settings::Key::General_Tags);
    data["General"][StrMaxCars.data()] = Application::Settings.getAsInt(Settings::Key::General_MaxCars);
    data["General"][StrMaxPlayers.data()] = Application::Settings.getAsInt(Settings::Key::General_MaxPlayers);
    data["General"][StrMap.data()] = Application::Settings.getAsString(Settings::Key::General_Map);
    data["General"][StrDescription.data()] = Application::Settings.getAsString(Settings::Key::General_Description);
    data["General"][StrResourceFolder.data()] = Application::Settings.getAsString(Settings::Key::General_ResourceFolder);
    // data["General"][StrPassword.data()] = Application::Settings.Password;
    // SetComment(data["General"][StrPassword.data()].comments(), " Sets a password on this server, which restricts people from joining. To join, a player must enter this exact password. Leave empty ("") to disable the password.");
    // Misc
    data["Misc"][StrHideUpdateMessages.data()] = Application::Settings.getAsBool(Settings::Key::Misc_ImScaredOfUpdates);
    SetComment(data["Misc"][StrHideUpdateMessages.data()].comments(), " Hides the periodic update message which notifies you of a new server version. You should really keep this on and always update as soon as possible. For more information visit https://wiki.beammp.com/en/home/server-maintenance#updating-the-server. An update message will always appear at startup regardless.");
    data["Misc"][StrSendErrors.data()] = Application::Settings.getAsBool(Settings::Key::Misc_SendErrors);
    data["Misc"][StrUpdateReminderTime.data()] = Application::Settings.getAsString(Settings::Key::Misc_UpdateReminderTime);
    SetComment(data["Misc"][StrUpdateReminderTime.data()].comments(), " Specifies the time between update reminders. You can use any of \"s, min, h, d\" at the end to specify the units seconds, minutes, hours or days. So 30d or 0.5min will print the update message every 30 days or half a minute.");
    SetComment(data["Misc"][StrSendErrors.data()].comments(), " If SendErrors is `true`, the server will send helpful info about crashes and other issues back to the BeamMP developers. This info may include your config, who is on your server at the time of the error, and similar general information. This kind of data is vital in helping us diagnose and fix issues faster. This has no impact on server performance. You can opt-out of this system by setting this to `false`");
    data["Misc"][StrSendErrorsMessageEnabled.data()] = Application::Settings.getAsBool(Settings::Key::Misc_SendErrorsShowMessage);
    SetComment(data["Misc"][StrSendErrorsMessageEnabled.data()].comments(), " You can turn on/off the SendErrors message you get on startup here");
    std::stringstream Ss;
    Ss << "# This is the BeamMP-Server config file.\n"
          "# Help & Documentation: `https://docs.beammp.com/server/server-maintenance/`\n"
          "# IMPORTANT: Fill in the AuthKey with the key you got from `https://keymaster.beammp.com/` on the left under \"Keys\"\n"
       << toml::format(data);
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
    FlushToFile();
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
        if (const char* envp = std::getenv(Env.data());
            envp != nullptr && std::strcmp(envp, "") != 0) {

            std::visit(
                overloaded {
                    [&envp, &key](std::string) {
                        Application::Settings.set(key, std::string(envp));
                    },
                    [&envp, &key](int) {
                        Application::Settings.set(key, int(std::strtol(envp, nullptr, 10)));
                    },
                    [&envp, &key](bool) {
                        auto Str = std::string(envp);
                        Application::Settings.set(key, bool(Str == "1" || Str == "true"));
                    } },

                Application::Settings.get(key));
            return;
        }
    }

    std::visit([&Table, &Category, &Key, &key](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::string>) {
            if (Table[Category.c_str()][Key.data()].is_string())
                Application::Settings.set(key, Table[Category.c_str()][Key.data()].as_string());
            else
                beammp_warnf("Value '{}.{}' has unexpected type, expected type 'string'", Category, Key);
        } else if constexpr (std::is_same_v<T, int>) {
            if (Table[Category.c_str()][Key.data()].is_integer())
                Application::Settings.set(key, int(Table[Category.c_str()][Key.data()].as_integer()));
            else
                beammp_warnf("Value '{}.{}' has unexpected type, expected type 'integer'", Category, Key);
        } else if constexpr (std::is_same_v<T, bool>) {
            if (Table[Category.c_str()][Key.data()].is_boolean())
                Application::Settings.set(key, Table[Category.c_str()][Key.data()].as_boolean());
            else
                beammp_warnf("Value '{}.{}' has unexpected type, expected type 'boolean'", Category, Key);
        } else {
            throw std::logic_error { "Invalid type for config value during read attempt" };
        }
    },
        Application::Settings.get(key));
}

void TConfig::ParseFromFile(std::string_view name) {
    try {
        toml::value data {};
        if (!mDisableConfig) {
            data = toml::parse(name.data());
        }

        // GENERAL

        // Read into new Settings Singleton
        TryReadValue(data, "General", StrDebug, EnvStrDebug, Settings::Key::General_Debug);
        TryReadValue(data, "General", StrPrivate, EnvStrPrivate, Settings::Key::General_Private);
        TryReadValue(data, "General", StrInformationPacket, EnvStrInformationPacket, Settings::Key::General_InformationPacket);
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
        TryReadValue(data, "General", StrAllowGuests, EnvStrAllowGuests, Settings::Key::General_AllowGuests);
        // Misc
        TryReadValue(data, "Misc", StrSendErrors, "", Settings::Key::Misc_SendErrors);
        TryReadValue(data, "Misc", StrHideUpdateMessages, "", Settings::Key::Misc_ImScaredOfUpdates);
        TryReadValue(data, "Misc", StrSendErrorsMessageEnabled, "", Settings::Key::Misc_SendErrorsShowMessage);
        TryReadValue(data, "Misc", StrUpdateReminderTime, "", Settings::Key::Misc_UpdateReminderTime);

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
    if (Application::Settings.getAsString(Settings::Key::General_AuthKey).empty()) {
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
    if (Application::Settings.getAsString(Settings::Key::General_AuthKey).size() != 36) {
        beammp_warn("AuthKey specified is the wrong length and likely isn't valid.");
    }
}

void TConfig::PrintDebug() {
    if (mDisableConfig) {
        beammp_debug("Provider turned off the generation and parsing of the ServerConfig.toml");
    }
    beammp_debug(std::string(StrDebug) + ": " + std::string(Application::Settings.getAsBool(Settings::Key::General_Debug) ? "true" : "false"));
    beammp_debug(std::string(StrPrivate) + ": " + std::string(Application::Settings.getAsBool(Settings::Key::General_Private) ? "true" : "false"));
    beammp_debug(std::string(StrInformationPacket) + ": " + std::string(Application::Settings.getAsBool(Settings::Key::General_InformationPacket) ? "true" : "false"));
    beammp_debug(std::string(StrPort) + ": " + std::to_string(Application::Settings.getAsInt(Settings::Key::General_Port)));
    beammp_debug(std::string(StrMaxCars) + ": " + std::to_string(Application::Settings.getAsInt(Settings::Key::General_MaxCars)));
    beammp_debug(std::string(StrMaxPlayers) + ": " + std::to_string(Application::Settings.getAsInt(Settings::Key::General_MaxPlayers)));
    beammp_debug(std::string(StrMap) + ": \"" + Application::Settings.getAsString(Settings::Key::General_Map) + "\"");
    beammp_debug(std::string(StrName) + ": \"" + Application::Settings.getAsString(Settings::Key::General_Name) + "\"");
    beammp_debug(std::string(StrDescription) + ": \"" + Application::Settings.getAsString(Settings::Key::General_Description) + "\"");
    beammp_debug(std::string(StrTags) + ": " + TagsAsPrettyArray());
    beammp_debug(std::string(StrLogChat) + ": \"" + (Application::Settings.getAsBool(Settings::Key::General_LogChat) ? "true" : "false") + "\"");
    beammp_debug(std::string(StrResourceFolder) + ": \"" + Application::Settings.getAsString(Settings::Key::General_ResourceFolder) + "\"");
    beammp_debug(std::string(StrAllowGuests) + ": \"" + (Application::Settings.getAsBool(Settings::Key::General_AllowGuests) ? "true" : "false") + "\"");
    // special!
    beammp_debug("Key Length: " + std::to_string(Application::Settings.getAsString(Settings::Key::General_AuthKey).length()) + "");
}

std::string TConfig::TagsAsPrettyArray() const {
    std::vector<std::string> TagsArray = {};
    SplitString(Application::Settings.getAsString(Settings::General_Tags), ',', TagsArray);
    std::string Pretty = {};
    for (size_t i = 0; i < TagsArray.size() - 1; ++i) {
        Pretty += '\"' + TagsArray[i] + "\", ";
    }
    Pretty += '\"' + TagsArray.at(TagsArray.size() - 1) + "\"";
    return Pretty;
}
