#include "Common.h"
#define TOML11_PRESERVE_COMMENTS_BY_DEFAULT

#include <toml11/toml.hpp> // header-only version of TOML++

#include "TConfig.h"
#include <fstream>
#include <iostream>
#include <istream>
#include <sstream>

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
static constexpr std::string_view StrSendErrors = "SendErrors";
static constexpr std::string_view StrSendErrorsMessageEnabled = "SendErrorsShowMessage";
static constexpr std::string_view StrSSLKeyPath = "SSLKeyPath";
static constexpr std::string_view StrSSLCertPath = "SSLCertPath";
static constexpr std::string_view StrHTTPServerPort = "HTTPServerPath";

void WriteSendErrors(const std::string& name) {
    std::ofstream CfgFile { name, std::ios::out | std::ios::app };
    CfgFile << "# You can turn on/off the SendErrors message you get on startup here" << std::endl
            << StrSendErrorsMessageEnabled << " = true" << std::endl
            << "# If SendErrors is `true`, the server will send helpful info about crashes and other issues back to the BeamMP developers. This info may include your config, who is on your server at the time of the error, and similar general information. This kind of data is vital in helping us diagnose and fix issues faster. This has no impact on server performance. You can opt-out of this system by setting this to `false`."
            << std::endl
            << StrSendErrors << " = true" << std::endl;
}

TConfig::TConfig(const std::string& ConfigFileName)
    : mConfigFileName(ConfigFileName) {
    if (!fs::exists(mConfigFileName) || !fs::is_regular_file(mConfigFileName)) {
        beammp_info("No config file found! Generating one...");
        CreateConfigFile(mConfigFileName);
    }
    if (!mFailed) {
        if (fs::exists("Server.cfg")) {
            beammp_warn("An old \"Server.cfg\" file still exists. Please note that this is no longer used. Instead, \"" + std::string(mConfigFileName) + "\" is used. You can safely delete the \"Server.cfg\".");
        }
        ParseFromFile(mConfigFileName);
    }
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
    auto data = toml::parse(mConfigFileName);
    data["General"] = toml::table();
    data["General"][StrAuthKey.data()] = Application::Settings.Key;
    data["General"][StrDebug.data()] = Application::Settings.DebugModeEnabled;
    data["General"][StrPrivate.data()] = Application::Settings.Private;
    data["General"][StrPort.data()] = Application::Settings.Port;
    data["General"][StrName.data()] = Application::Settings.ServerName;
    data["General"][StrMaxCars.data()] = Application::Settings.MaxCars;
    data["General"][StrMaxPlayers.data()] = Application::Settings.MaxPlayers;
    data["General"][StrMap.data()] = Application::Settings.MapName;
    data["General"][StrDescription.data()] = Application::Settings.ServerDesc;
    data["General"][StrResourceFolder.data()] = Application::Settings.Resource;
    data["General"][StrSendErrors.data()] = Application::Settings.SendErrors;
    data["General"][StrSendErrorsMessageEnabled.data()] = Application::Settings.SendErrorsMessageEnabled;
    data["General"][StrSSLKeyPath.data()] = Application::Settings.SSLKeyPath;
    data["General"][StrSSLCertPath.data()] = Application::Settings.SSLCertPath;
    data["General"][StrHTTPServerPort.data()] = Application::Settings.HTTPServerPort;
    std::ofstream Stream(mConfigFileName);
    Stream << data << std::flush;
}

void TConfig::CreateConfigFile(std::string_view name) {
    // build from old config Server.cfg

    try {
        if (fs::exists("Server.cfg")) {
            // parse it (this is weird and bad and should be removed in some future version)
            ParseOldFormat();
        }
    } catch (const std::exception& e) {
        beammp_error("an error occurred and was ignored during config transfer: " + std::string(e.what()));
    }

    { // create file context
        std::ofstream ofs(name.data());
    }

    auto data = toml::parse<toml::preserve_comments>(name.data());
    //{ StrSendErrors, Application::Settings.SendErrors },

    data["General"] = toml::table();
    data["General"][StrAuthKey.data()] = Application::Settings.Key;
    data["General"][StrDebug.data()] = Application::Settings.DebugModeEnabled;
    data["General"][StrPrivate.data()] = Application::Settings.Private;
    data["General"][StrPort.data()] = Application::Settings.Port;
    data["General"][StrName.data()] = Application::Settings.ServerName;
    data["General"][StrMaxCars.data()] = Application::Settings.MaxCars;
    data["General"][StrMaxPlayers.data()] = Application::Settings.MaxPlayers;
    data["General"][StrMap.data()] = Application::Settings.MapName;
    data["General"][StrDescription.data()] = Application::Settings.ServerDesc;
    data["General"][StrResourceFolder.data()] = Application::Settings.Resource;
    data["General"][StrSSLKeyPath.data()] = Application::Settings.SSLKeyPath;
    data["General"][StrSSLCertPath.data()] = Application::Settings.SSLCertPath;
    data["General"][StrHTTPServerPort.data()] = Application::Settings.HTTPServerPort;

    std::ofstream ofs { std::string(name) };
    if (ofs.good()) {
        ofs << "# This is the BeamMP-Server config file.\n"
               "# Help & Documentation: `https://wiki.beammp.com/en/home/server-maintenance`\n"
               "# IMPORTANT: Fill in the AuthKey with the key you got from `https://beammp.com/k/dashboard` on the left under \"Keys\"\n"
            << '\n';
        ofs << data << '\n';
        beammp_error("There was no \"" + std::string(mConfigFileName) + "\" file (this is normal for the first time running the server), so one was generated for you. It was automatically filled with the settings from your Server.cfg, if you have one. Please open ServerConfig.toml and ensure your AuthKey and other settings are filled in and correct, then restart the server. The old Server.cfg file will no longer be used and causes a warning if it exists from now on.");
        mFailed = true;
        ofs.close();
        WriteSendErrors(std::string(name));
    } else {
        beammp_error("Couldn't create " + std::string(name) + ". Check permissions, try again, and contact support if it continues not to work.");
        mFailed = true;
    }
}

void TConfig::ParseFromFile(std::string_view name) {
    try {
        toml::value data = toml::parse<toml::preserve_comments>(name.data());
        Application::Settings.DebugModeEnabled = data["General"][StrDebug.data()].as_boolean();
        Application::Settings.Private = data["General"][StrPrivate.data()].as_boolean();
        Application::Settings.Port = data["General"][StrPort.data()].as_integer();
        Application::Settings.MaxCars = data["General"][StrMaxCars.data()].as_integer();
        Application::Settings.MaxPlayers = data["General"][StrMaxPlayers.data()].as_integer();
        Application::Settings.MapName = data["General"][StrMap.data()].as_string();
        Application::Settings.ServerName = data["General"][StrName.data()].as_string();
        Application::Settings.ServerDesc = data["General"][StrDescription.data()].as_string();
        Application::Settings.Resource = data["General"][StrResourceFolder.data()].as_string();
        Application::Settings.Key = data["General"][StrAuthKey.data()].as_string();
        Application::Settings.SSLKeyPath = data["General"][StrSSLKeyPath.data()].as_string();
        Application::Settings.SSLCertPath = data["General"][StrSSLCertPath.data()].as_string();
        Application::Settings.HTTPServerPort = data["General"][StrHTTPServerPort.data()].as_string();
        if (!data["General"][StrSendErrors.data()].is_boolean()
            || !data["General"][StrSendErrorsMessageEnabled.data()].is_boolean()) {
            WriteSendErrors(std::string(name));
        } else {
            Application::Settings.SendErrors = data["General"][StrSendErrors.data()].as_boolean();
            Application::Settings.SendErrorsMessageEnabled = data["General"][StrSendErrorsMessageEnabled.data()].as_boolean();
        }
    } catch (const std::exception& err) {
        beammp_error("Error parsing config file value: " + std::string(err.what()));
        mFailed = true;
        return;
    }
    PrintDebug();
    // all good so far, let's check if there's a key
    if (Application::Settings.Key.empty()) {
        beammp_error("No AuthKey specified in the \"" + std::string(mConfigFileName) + "\" file. Please get an AuthKey, enter it into the config file, and restart this server.");
        mFailed = true;
    }
}

void TConfig::PrintDebug() {
    beammp_debug(std::string(StrDebug) + ": " + std::string(Application::Settings.DebugModeEnabled ? "true" : "false"));
    beammp_debug(std::string(StrPrivate) + ": " + std::string(Application::Settings.Private ? "true" : "false"));
    beammp_debug(std::string(StrPort) + ": " + std::to_string(Application::Settings.Port));
    beammp_debug(std::string(StrMaxCars) + ": " + std::to_string(Application::Settings.MaxCars));
    beammp_debug(std::string(StrMaxPlayers) + ": " + std::to_string(Application::Settings.MaxPlayers));
    beammp_debug(std::string(StrMap) + ": \"" + Application::Settings.MapName + "\"");
    beammp_debug(std::string(StrName) + ": \"" + Application::Settings.ServerName + "\"");
    beammp_debug(std::string(StrDescription) + ": \"" + Application::Settings.ServerDesc + "\"");
    beammp_debug(std::string(StrResourceFolder) + ": \"" + Application::Settings.Resource + "\"");
    beammp_debug(std::string(StrSSLKeyPath) + ": \"" + Application::Settings.SSLKeyPath + "\"");
    beammp_debug(std::string(StrSSLCertPath) + ": \"" + Application::Settings.SSLCertPath + "\"");
    // special!
    beammp_debug("Key Length: " + std::to_string(Application::Settings.Key.length()) + "");
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
