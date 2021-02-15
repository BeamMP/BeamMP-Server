#include "../include/TConfig.h"
#include <fstream>

TConfig::TConfig(const std::string& ConfigFile) {
    std::ifstream File(ConfigFile);
    if (File.good()) {
        std::string line;
        int index = 1;
        while (getline(File, line)) {
            index++;
        }
        if (index - 1 < 11) {
            error(("Outdated/Incorrect config please remove it server will close in 5 secs"));
            std::this_thread::sleep_for(std::chrono::seconds(3));
            _Exit(0);
        }
        File.close();
        File.open(("Server.cfg"));
        info(("Config found updating values"));
        index = 1;
        while (std::getline(File, line)) {
            if (line.rfind('#', 0) != 0 && line.rfind(' ', 0) != 0) { //Checks if it starts as Comment
                std::string CleanLine = RemoveComments(line); //Cleans it from the Comments
                SetValues(CleanLine, index); //sets the values
                index++;
            }
        }
    } else {
        info(("Config not found generating default"));
        std::ofstream FileStream;
        FileStream.open(ConfigFile);
        // TODO REPLACE THIS SHIT OMG
        FileStream << ("# This is the BeamMP Server Configuration File v0.60\n"
                       "Debug = false # true or false to enable debug console output\n"
                       "Private = true # Private?\n"
                       "Port = 30814 # Port to run the server on UDP and TCP\n"
                       "Cars = 1 # Max cars for every player\n"
                       "MaxPlayers = 10 # Maximum Amount of Clients\n"
                       "Map = \"/levels/gridmap/info.json\" # Default Map\n"
                       "Name = \"BeamMP New Server\" # Server Name\n"
                       "Desc = \"BeamMP Default Description\" # Server Description\n"
                       "use = \"Resources\" # Resource file name\n"
                       "AuthKey = \"\" # Auth Key");
        FileStream.close();
        error(("You are required to input the AuthKey"));
        std::this_thread::sleep_for(std::chrono::seconds(3));
        _Exit(0);
    }
    debug("Debug      : " + std::string(Application::Settings.DebugModeEnabled ? "true" : "false"));
    debug("Private    : " + std::string(Application::Settings.Private ? "true" : "false"));
    debug("Port       : " + std::to_string(Application::Settings.Port));
    debug("Max Cars   : " + std::to_string(Application::Settings.MaxCars));
    debug("MaxPlayers : " + std::to_string(Application::Settings.MaxPlayers));
    debug("MapName    : \"" + Application::Settings.MapName + "\"");
    debug("ServerName : \"" + Application::Settings.ServerName + "\"");
    debug("ServerDesc : \"" + Application::Settings.ServerDesc + "\"");
    debug("File       : \"" + Application::Settings.Resource + "\"");
    debug("Key length : " + std::to_string(Application::Settings.Key.length()) + "");
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
