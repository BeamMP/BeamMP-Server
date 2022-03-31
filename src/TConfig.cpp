#include "Common.h"

#include "TConfig.h"
#include <fstream>
#include <iostream>
#include <istream>
#include <sstream>

// General
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
static constexpr std::string_view StrLogChat = "LogChat";

// Misc
static constexpr std::string_view StrSendErrors = "SendErrors";
static constexpr std::string_view StrSendErrorsMessageEnabled = "SendErrorsShowMessage";
static constexpr std::string_view StrHideUpdateMessages = "ImScaredOfUpdates";

// HTTP
static constexpr std::string_view StrHTTPServerEnabled = "HTTPServerEnabled";
static constexpr std::string_view StrHTTPServerUseSSL = "UseSSL";
static constexpr std::string_view StrSSLKeyPath = "SSLKeyPath";
static constexpr std::string_view StrSSLCertPath = "SSLCertPath";
static constexpr std::string_view StrHTTPServerPort = "HTTPServerPort";

TConfig::TConfig(const std::string& ConfigFileName)
    : mConfigFileName(ConfigFileName) {
    Application::SetSubsystemStatus("Config", Application::Status::Starting);
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
    auto data = toml::parse<toml::preserve_comments>(mConfigFileName);
    data["General"][StrAuthKey.data()] = Application::Settings.Key;
    SetComment(data["General"][StrAuthKey.data()].comments(), " AuthKey has to be filled out in order to run the server");
    data["General"][StrLogChat.data()] = Application::Settings.LogChat;
    SetComment(data["General"][StrLogChat.data()].comments(), " Whether to log chat messages in the console / log");
    data["General"][StrDebug.data()] = Application::Settings.DebugModeEnabled;
    data["General"][StrPrivate.data()] = Application::Settings.Private;
    data["General"][StrPort.data()] = Application::Settings.Port;
    data["General"][StrName.data()] = Application::Settings.ServerName;
    data["General"][StrMaxCars.data()] = Application::Settings.MaxCars;
    data["General"][StrMaxPlayers.data()] = Application::Settings.MaxPlayers;
    data["General"][StrMap.data()] = Application::Settings.MapName;
    data["General"][StrDescription.data()] = Application::Settings.ServerDesc;
    data["General"][StrResourceFolder.data()] = Application::Settings.Resource;
    // Misc
    data["Misc"][StrHideUpdateMessages.data()] = Application::Settings.HideUpdateMessages;
    SetComment(data["Misc"][StrHideUpdateMessages.data()].comments(), " Hides the periodic update message which notifies you of a new server version. You should really keep this on and always update as soon as possible. For more information visit https://wiki.beammp.com/en/home/server-maintenance#updating-the-server. An update message will always appear at startup regardless.");
    data["Misc"][StrSendErrors.data()] = Application::Settings.SendErrors;
    SetComment(data["Misc"][StrSendErrors.data()].comments(), " You can turn on/off the SendErrors message you get on startup here");
    data["Misc"][StrSendErrorsMessageEnabled.data()] = Application::Settings.SendErrorsMessageEnabled;
    SetComment(data["Misc"][StrSendErrorsMessageEnabled.data()].comments(), " If SendErrors is `true`, the server will send helpful info about crashes and other issues back to the BeamMP developers. This info may include your config, who is on your server at the time of the error, and similar general information. This kind of data is vital in helping us diagnose and fix issues faster. This has no impact on server performance. You can opt-out of this system by setting this to `false`");
    // HTTP
    data["HTTP"][StrSSLKeyPath.data()] = Application::Settings.SSLKeyPath;
    data["HTTP"][StrSSLCertPath.data()] = Application::Settings.SSLCertPath;
    data["HTTP"][StrHTTPServerPort.data()] = Application::Settings.HTTPServerPort;
    data["HTTP"][StrHTTPServerUseSSL.data()] = Application::Settings.HTTPServerUseSSL;
    SetComment(data["HTTP"][StrHTTPServerUseSSL.data()].comments(), " Recommended to keep enabled. With SSL the server will serve https and requires valid key and cert files");
    data["HTTP"][StrHTTPServerEnabled.data()] = Application::Settings.HTTPServerEnabled;
    SetComment(data["HTTP"][StrHTTPServerEnabled.data()].comments(), " Enables the internal HTTP server");
    std::ofstream Stream(mConfigFileName, std::ios::trunc | std::ios::out);
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

    FlushToFile();

    size_t FileSize = fs::file_size(name);
    std::fstream ofs { std::string(name), std::ios::in | std::ios::out };
    if (ofs.good()) {
        std::string Contents {};
        Contents.resize(FileSize);
        ofs.readsome(Contents.data(), FileSize);
        ofs.seekp(0);
        ofs << "# This is the BeamMP-Server config file.\n"
               "# Help & Documentation: `https://wiki.beammp.com/en/home/server-maintenance`\n"
               "# IMPORTANT: Fill in the AuthKey with the key you got from `https://beammp.com/k/dashboard` on the left under \"Keys\"\n"
            << '\n'
            << Contents;
        beammp_error("There was no \"" + std::string(mConfigFileName) + "\" file (this is normal for the first time running the server), so one was generated for you. It was automatically filled with the settings from your Server.cfg, if you have one. Please open ServerConfig.toml and ensure your AuthKey and other settings are filled in and correct, then restart the server. The old Server.cfg file will no longer be used and causes a warning if it exists from now on.");
        mFailed = true;
        ofs.close();
    } else {
        beammp_error("Couldn't create " + std::string(name) + ". Check permissions, try again, and contact support if it continues not to work.");
        Application::SetSubsystemStatus("Config", Application::Status::Bad);
        mFailed = true;
    }
}

void TConfig::TryReadValue(toml::value& Table, const std::string& Category, const std::string_view& Key, std::string& OutValue) {
    if (Table[Category.c_str()][Key.data()].is_string()) {
        OutValue = Table[Category.c_str()][Key.data()].as_string();
    }
}

void TConfig::TryReadValue(toml::value& Table, const std::string& Category, const std::string_view& Key, bool& OutValue) {
    if (Table[Category.c_str()][Key.data()].is_boolean()) {
        OutValue = Table[Category.c_str()][Key.data()].as_boolean();
    }
}

void TConfig::TryReadValue(toml::value& Table, const std::string& Category, const std::string_view& Key, int& OutValue) {
    if (Table[Category.c_str()][Key.data()].is_integer()) {
        OutValue = Table[Category.c_str()][Key.data()].as_integer();
    }
}

void TConfig::ParseFromFile(std::string_view name) {
    try {
        toml::value data = toml::parse<toml::preserve_comments>(name.data());
        // GENERAL
        TryReadValue(data, "General", StrDebug, Application::Settings.DebugModeEnabled);
        TryReadValue(data, "General", StrPrivate, Application::Settings.Private);
        TryReadValue(data, "General", StrPort, Application::Settings.Port);
        TryReadValue(data, "General", StrMaxCars, Application::Settings.MaxCars);
        TryReadValue(data, "General", StrMaxPlayers, Application::Settings.MaxPlayers);
        TryReadValue(data, "General", StrMap, Application::Settings.MapName);
        TryReadValue(data, "General", StrName, Application::Settings.ServerName);
        TryReadValue(data, "General", StrDescription, Application::Settings.ServerDesc);
        TryReadValue(data, "General", StrResourceFolder, Application::Settings.Resource);
        TryReadValue(data, "General", StrAuthKey, Application::Settings.Key);
        TryReadValue(data, "General", StrLogChat, Application::Settings.LogChat);
        // Misc
        TryReadValue(data, "Misc", StrSendErrors, Application::Settings.SendErrors);
        TryReadValue(data, "Misc", StrHideUpdateMessages, Application::Settings.HideUpdateMessages);
        TryReadValue(data, "Misc", StrSendErrorsMessageEnabled, Application::Settings.SendErrorsMessageEnabled);
        // HTTP
        TryReadValue(data, "HTTP", StrSSLKeyPath, Application::Settings.SSLKeyPath);
        TryReadValue(data, "HTTP", StrSSLCertPath, Application::Settings.SSLCertPath);
        TryReadValue(data, "HTTP", StrHTTPServerPort, Application::Settings.HTTPServerPort);
        TryReadValue(data, "HTTP", StrHTTPServerEnabled, Application::Settings.HTTPServerEnabled);
        TryReadValue(data, "HTTP", StrHTTPServerUseSSL, Application::Settings.HTTPServerUseSSL);
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
    if (Application::Settings.Key.empty()) {
        beammp_error("No AuthKey specified in the \"" + std::string(mConfigFileName) + "\" file. Please get an AuthKey, enter it into the config file, and restart this server.");
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
    beammp_debug(std::string(StrDebug) + ": " + std::string(Application::Settings.DebugModeEnabled ? "true" : "false"));
    beammp_debug(std::string(StrPrivate) + ": " + std::string(Application::Settings.Private ? "true" : "false"));
    beammp_debug(std::string(StrPort) + ": " + std::to_string(Application::Settings.Port));
    beammp_debug(std::string(StrMaxCars) + ": " + std::to_string(Application::Settings.MaxCars));
    beammp_debug(std::string(StrMaxPlayers) + ": " + std::to_string(Application::Settings.MaxPlayers));
    beammp_debug(std::string(StrMap) + ": \"" + Application::Settings.MapName + "\"");
    beammp_debug(std::string(StrName) + ": \"" + Application::Settings.ServerName + "\"");
    beammp_debug(std::string(StrDescription) + ": \"" + Application::Settings.ServerDesc + "\"");
    beammp_debug(std::string(StrLogChat) + ": \"" + (Application::Settings.LogChat ? "true" : "false") + "\"");
    beammp_debug(std::string(StrResourceFolder) + ": \"" + Application::Settings.Resource + "\"");
    beammp_debug(std::string(StrSSLKeyPath) + ": \"" + Application::Settings.SSLKeyPath + "\"");
    beammp_debug(std::string(StrSSLCertPath) + ": \"" + Application::Settings.SSLCertPath + "\"");
    beammp_debug(std::string(StrHTTPServerPort) + ": \"" + std::to_string(Application::Settings.HTTPServerPort) + "\"");
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
