#include "Common.h"

#include "TConfig.h"
#include <fstream>
#include <iostream>
#include <istream>
#include <sstream>

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
    CHECK(table.at("HTTP").is_table());

    fs::remove(CfgFile);
}

TConfig::TConfig(const std::string& ConfigFileName)
    : mConfigFileName(ConfigFileName) {
    Application::SetSubsystemStatus("Config", Application::Status::Starting);
    if (!fs::exists(mConfigFileName) || !fs::is_regular_file(mConfigFileName)) {
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
    data["General"][StrAuthKey.data()] = Application::GetSettingString(StrAuthKey.data());
    SetComment(data["General"][StrAuthKey.data()].comments(), " AuthKey has to be filled out in order to run the server");
    data["General"][StrLogChat.data()] = Application::GetSettingBool(StrLogChat.data());
    SetComment(data["General"][StrLogChat.data()].comments(), " Whether to log chat messages in the console / log");
    data["General"][StrDebug.data()] = Application::GetSettingBool(StrDebug.data());
    data["General"][StrPrivate.data()] = Application::GetSettingBool(StrPrivate.data());
    data["General"][StrPort.data()] = Application::GetSettingInt(StrPort.data());
    data["General"][StrName.data()] = Application::GetSettingString(StrName.data());
    data["General"][StrMaxCars.data()] = Application::GetSettingInt(StrMaxCars.data());
    data["General"][StrMaxPlayers.data()] = Application::GetSettingInt(StrMaxPlayers.data());
    data["General"][StrMap.data()] = Application::GetSettingString(StrMap.data());
    data["General"][StrDescription.data()] = Application::GetSettingString(StrDescription.data());
    data["General"][StrResourceFolder.data()] = Application::GetSettingString(StrResourceFolder.data());
    // Misc
    data["Misc"][StrHideUpdateMessages.data()] = Application::GetSettingBool(StrHideUpdateMessages.data());
    SetComment(data["Misc"][StrHideUpdateMessages.data()].comments(), " Hides the periodic update message which notifies you of a new server version. You should really keep this on and always update as soon as possible. For more information visit https://wiki.beammp.com/en/home/server-maintenance#updating-the-server. An update message will always appear at startup regardless.");
    data["Misc"][StrSendErrors.data()] = Application::GetSettingBool(StrSendErrors.data());
    SetComment(data["Misc"][StrSendErrors.data()].comments(), " You can turn on/off the SendErrors message you get on startup here");
    data["Misc"][StrSendErrorsMessageEnabled.data()] = Application::GetSettingBool(StrSendErrorsMessageEnabled.data());
    SetComment(data["Misc"][StrSendErrorsMessageEnabled.data()].comments(), " If SendErrors is `true`, the server will send helpful info about crashes and other issues back to the BeamMP developers. This info may include your config, who is on your server at the time of the error, and similar general information. This kind of data is vital in helping us diagnose and fix issues faster. This has no impact on server performance. You can opt-out of this system by setting this to `false`");
    // HTTP
    data["HTTP"][StrSSLKeyPath.data()] = Application::GetSettingString(StrSSLKeyPath.data());
    data["HTTP"][StrSSLCertPath.data()] = Application::GetSettingString(StrSSLCertPath.data());
    data["HTTP"][StrHTTPServerPort.data()] = Application::GetSettingInt(StrHTTPServerPort.data());
    SetComment(data["HTTP"][StrHTTPServerIP.data()].comments(), " Which IP to listen on. Pick 0.0.0.0 for a public-facing server with no specific IP, and 127.0.0.1 or 'localhost' for a local server.");
    data["HTTP"][StrHTTPServerIP.data()] = Application::GetSettingString(StrHTTPServerIP.data());
    data["HTTP"][StrHTTPServerUseSSL.data()] = Application::GetSettingBool(StrHTTPServerUseSSL.data());
    SetComment(data["HTTP"][StrHTTPServerUseSSL.data()].comments(), " Recommended to have enabled for servers which face the internet. With SSL the server will serve https and requires valid key and cert files");
    data["HTTP"][StrHTTPServerEnabled.data()] = Application::GetSettingBool(StrHTTPServerEnabled.data());
    SetComment(data["HTTP"][StrHTTPServerEnabled.data()].comments(), " Enables the internal HTTP server");
    std::stringstream Ss;
    Ss << "# This is the BeamMP-Server config file.\n"
          "# Help & Documentation: `https://wiki.beammp.com/en/home/server-maintenance`\n"
          "# IMPORTANT: Fill in the AuthKey with the key you got from `https://beammp.com/k/dashboard` on the left under \"Keys\"\n"
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

void TConfig::TryReadValue(toml::value& Table, const std::string& Category, const std::string_view& Key) {
    if (Table[Category.c_str()][Key.data()].is_string()) {
        Application::SetSetting(Key, std::string(Table[Category.c_str()][Key.data()].as_string()));
    } else if (Table[Category.c_str()][Key.data()].is_boolean()) {
        Application::SetSetting(Key, bool(Table[Category.c_str()][Key.data()].as_boolean()));
    } else if (Table[Category.c_str()][Key.data()].is_integer()) {
        Application::SetSetting(Key, int(Table[Category.c_str()][Key.data()].as_integer()));
    }
}
void TConfig::ParseFromFile(std::string_view name) {
    try {
        toml::value data = toml::parse<toml::preserve_comments>(name.data());
        // GENERAL
        TryReadValue(data, "General", StrDebug);
        TryReadValue(data, "General", StrPrivate);
        TryReadValue(data, "General", StrPort);
        TryReadValue(data, "General", StrMaxCars);
        TryReadValue(data, "General", StrMaxPlayers);
        TryReadValue(data, "General", StrMap);
        TryReadValue(data, "General", StrName);
        TryReadValue(data, "General", StrDescription);
        TryReadValue(data, "General", StrResourceFolder);
        TryReadValue(data, "General", StrAuthKey);
        TryReadValue(data, "General", StrLogChat);
        // Misc
        TryReadValue(data, "Misc", StrSendErrors);
        TryReadValue(data, "Misc", StrHideUpdateMessages);
        TryReadValue(data, "Misc", StrSendErrorsMessageEnabled);
        // HTTP
        TryReadValue(data, "HTTP", StrSSLKeyPath);
        TryReadValue(data, "HTTP", StrSSLCertPath);
        TryReadValue(data, "HTTP", StrHTTPServerPort);
        TryReadValue(data, "HTTP", StrHTTPServerIP);
        TryReadValue(data, "HTTP", StrHTTPServerEnabled);
        TryReadValue(data, "HTTP", StrHTTPServerUseSSL);
    } catch (const std::exception& err) {
        beammp_error("Error parsing config file value: " + std::string(err.what()));
        mFailed = true;
        Application::SetSubsystemStatus("Config", Application::Status::Bad);
        return;
    }
    PrintDebug();

    // Update in any case
    FlushToFile();
    // all good so far, let's check if there's a key
    if (Application::GetSettingString(StrAuthKey).empty()) {
        beammp_error("No AuthKey specified in the \"" + std::string(mConfigFileName) + "\" file. Please get an AuthKey, enter it into the config file, and restart this server.");
        Application::SetSubsystemStatus("Config", Application::Status::Bad);
        mFailed = true;
        return;
    }
    Application::SetSubsystemStatus("Config", Application::Status::Good);
    if (Application::GetSettingString(StrAuthKey).size() != 36) {
        beammp_warn("AuthKey specified is the wrong length and likely isn't valid.");
    }
}

void TConfig::PrintDebug() {
    for (const auto& [k, v] : Application::mSettings) {
        if (k == StrAuthKey) {
            beammp_debugf("AuthKey: length {}", boost::get<std::string>(v).size());
            continue;
        }
        beammp_debugf("{}: {}", k, Application::SettingToString(v));
    }
}

void TConfig::ParseOldFormat() {
    beammp_warnf("You still have a 'Server.cfg' - this will not be used (this server uses 'ServerConfig.toml'. Since v3.0.2 we no longer parse and import these old settings. Remove the file to avoid confusion and disable this message.");
}
