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
    /*if (!fs::exists("Server.cfg")) {
        info("Config not found generating default");
        ManageJson();
        error("AuthKey cannot be empty check Config.json!");
        std::this_thread::sleep_for(std::chrono::seconds(3));
        Application::GracefullyShutdown();
        return;
    }

    std::ifstream File(ConfigFile);
    if (File.good()) {
        std::string line;
        int index = 1;
        while (getline(File, line)) {
            index++;
        }
        if (index - 1 < 11) {
            error("Outdated/Incorrect config please remove it server will close in 5 secs");
            std::this_thread::sleep_for(std::chrono::seconds(3));
            _Exit(0);
        }
        File.close();
        File.open("Server.cfg");
        info("Old Config found updating values");
        index = 1;
        while (std::getline(File, line)) {
            if (line.rfind('#', 0) != 0 && line.rfind(' ', 0) != 0) { //Checks if it starts as Comment
                std::string CleanLine = RemoveComments(line); //Cleans it from the Comments
                SetValues(CleanLine, index); //sets the values
                index++;
            }
        }
    } else {
        info("Config not found generating default");
        std::ofstream FileStream;
        FileStream.open(ConfigFile);
        // TODO REPLACE THIS SHIT OMG -- replacing
        FileStream << "# This is the BeamMP Server Configuration File v0.60\n"
                      "Debug = false # true or false to enable debug console output\n"
                      "Private = true # Private?\n"
                      "Port = 30814 # Port to run the server on UDP and TCP\n"
                      "Cars = 1 # Max cars for every player\n"
                      "MaxPlayers = 10 # Maximum Amount of Clients\n"
                      "Map = \"/levels/gridmap/info.json\" # Default Map\n"
                      "Name = \"BeamMP New Server\" # Server Name\n"
                      "Desc = \"BeamMP Default Description\" # Server Description\n"
                      "use = \"Resources\" # Resource file name\n"
                      "AuthKey = \"\" # Auth Key";
        FileStream.close();
        error("You are required to input the AuthKey");
        std::this_thread::sleep_for(std::chrono::seconds(3));
        _Exit(0);
    }

     */
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
               "# IMPORTANT: Fill in the AuthKey with the key you got from `https://beammp.com/k/dashboard` on the left under \"Keys\""
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

/*

void TConfig::ReadJson() {
    auto Size = fs::file_size("Config.json");
    if (Size < 3)
        return;

    std::ifstream ifs("Config.json");
    if (ifs.is_open()) {
        std::string cfg(Size, 0);
        ifs.read(&cfg[0], long(Size));
        ifs.close();

        if (auto pos = cfg.find('{'); pos != std::string::npos) {
            if (cfg.at(0) != '{') {
                cfg = cfg.substr(pos);
            }
        } else {
            error("Config file is not valid JSON!");
            std::this_thread::sleep_for(std::chrono::seconds(3));
            Application::GracefullyShutdown();
        }

        rapidjson::Document d;
        d.Parse(cfg.c_str());
        if (!d.HasParseError()) {
            auto& Val = d["Debug"];
            if (!Val.IsNull() && Val.IsBool()) {
                Application::Settings.DebugModeEnabled = Val.GetBool();
            } else {
                info("'Debug' Missing in config! Setting to 'false' by default");
            }

            Val = d["Private"];

            if (!Val.IsNull() && Val.IsBool()) {
                Application::Settings.Private = Val.GetBool();
            } else {
                info("'Private' Missing in config! Setting to 'true' by default");
            }

            Val = d["Port"];

            if (!Val.IsNull() && Val.IsNumber()) {
                Application::Settings.Port = Val.GetInt();
            } else {
                info("'Port' Missing in config! Setting to '30814' by default");
            }

            Val = d["MaxCars"];

            if (!Val.IsNull() && Val.IsNumber()) {
                Application::Settings.MaxCars = Val.GetInt();
            } else {
                info("'MaxCars' Missing in config! Setting to '1' by default");
            }

            Val = d["MaxPlayers"];

            if (!Val.IsNull() && Val.IsNumber()) {
                Application::Settings.MaxPlayers = Val.GetInt();
            } else {
                info("'MaxPlayers' Missing in config! Setting to '10' by default");
            }

            Val = d["Map"];

            if (!Val.IsNull() && Val.IsString()) {
                Application::Settings.MapName = Val.GetString();
            } else {
                info("'Map' Missing in config! Setting to '/levels/gridmap/info.json' by default");
            }

            Val = d["Name"];

            if (!Val.IsNull() && Val.IsString()) {
                Application::Settings.ServerName = Val.GetString();
            } else {
                info("'Name' Missing in config! Setting to 'BeamMP Server' by default");
            }

            Val = d["Desc"];

            if (!Val.IsNull() && Val.IsString()) {
                Application::Settings.ServerDesc = Val.GetString();
            } else {
                info("'Desc' Missing in config! Setting to 'BeamMP Default Description' by default");
            }

            Val = d["Resource"];

            if (!Val.IsNull() && Val.IsString()) {
                Application::Settings.Resource = Val.GetString();
            } else {
                info("'Resource' Missing in config! Setting to 'Resources' by default");
            }

            Val = d["AuthKey"];

            if (!Val.IsNull() && Val.IsString()) {
                Application::Settings.Key = Val.GetString();
                if (Application::Settings.Key.empty()) {
                    error("AuthKey cannot be empty check Config.json!");
                    std::this_thread::sleep_for(std::chrono::seconds(3));
                    Application::GracefullyShutdown();
                }
            } else {
                error("'AuthKey' Missing in config!");
                std::this_thread::sleep_for(std::chrono::seconds(3));
                Application::GracefullyShutdown();
            }

        } else {
            error("Failed to parse JSON config! code " + std::to_string(d.GetParseError()));
        }
    } else {
        error("Failed to read Config.json");
        std::this_thread::sleep_for(std::chrono::seconds(3));
        Application::GracefullyShutdown();
    }
    PrintDebug();
}

void TConfig::ManageJson() {
    rapidjson::Document d;
    d.Parse("{}");
    d.AddMember("Debug", Application::Settings.DebugModeEnabled, d.GetAllocator());
    d.AddMember("Private", Application::Settings.Private, d.GetAllocator());
    d.AddMember("Port", Application::Settings.Port, d.GetAllocator());
    d.AddMember("MaxCars", Application::Settings.MaxCars, d.GetAllocator());
    d.AddMember("MaxPlayers", Application::Settings.MaxPlayers, d.GetAllocator());
    d.AddMember("Map", rapidjson::StringRef(Application::Settings.MapName.c_str()), d.GetAllocator());
    d.AddMember("Name", rapidjson::StringRef(Application::Settings.ServerName.c_str()), d.GetAllocator());
    d.AddMember("Desc", rapidjson::StringRef(Application::Settings.ServerDesc.c_str()), d.GetAllocator());
    d.AddMember("Resource", rapidjson::StringRef(Application::Settings.Resource.c_str()), d.GetAllocator());
    d.AddMember("AuthKey", rapidjson::StringRef(Application::Settings.Key.c_str()), d.GetAllocator());

    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    d.Accept(writer);

    std::ofstream cfg;
    cfg.open("Config.json");
    if (cfg.is_open()) {
        cfg << "BeamMP Server Configuration File\n"
            << buffer.GetString();
        cfg.close();
    } else {
        error("Failed to create Config.json!");
    }
}

std::string TConfig::RemoveComments(const std::string& Line) {
    std::string Return;
    for (char c : Line) {
        if (c == '#')
            break;
        Return += c;
    }
    return Return;
}

void TConfig::SetValues(const std::string& Line, int Index) {
    int state = 0;
    std::string Data;
    bool Switch = false;
    if (Index > 5)
        Switch = true;
    for (char c : Line) {
        if (Switch) {
            if (c == '\"')
                state++;
            if (state > 0 && state < 2)
                Data += c;
        } else {
            if (c == ' ')
                state++;
            if (state > 1)
                Data += c;
        }
    }
    Data = Data.substr(1);
    std::string::size_type sz;
    bool FoundTrue = std::string(Data).find("true") != std::string::npos; //searches for "true"
    switch (Index) {
    case 1:
        Application::Settings.DebugModeEnabled = FoundTrue; //checks and sets the Debug Value
        break;
    case 2:
        Application::Settings.Private = FoundTrue; //checks and sets the Private Value
        break;
    case 3:
        Application::Settings.Port = std::stoi(Data, &sz); //sets the Port
        break;
    case 4:
        Application::Settings.MaxCars = std::stoi(Data, &sz); //sets the Max Car amount
        break;
    case 5:
        Application::Settings.MaxPlayers = std::stoi(Data, &sz); //sets the Max Amount of player
        break;
    case 6:
        Application::Settings.MapName = Data; //Map
        break;
    case 7:
        Application::Settings.ServerName = Data; //Name
        break;
    case 8:
        Application::Settings.ServerDesc = Data; //desc
        break;
    case 9:
        Application::Settings.Resource = Data; //File name
        break;
    case 10:
        Application::Settings.Key = Data; //File name
    default:
        break;
    }
}
*/
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
