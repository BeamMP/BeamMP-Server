// Copyright (c) 2019-present Anonymous275.
// BeamMP Server code is not in the public domain and is not free software.
// One must be granted explicit permission by the copyright holder in order to modify or distribute any part of the source or binaries.
// Anything else is prohibited. Modified works may not be published and have be upstreamed to the official repository.
///
/// Created by Anonymous275 on 7/28/2020
///
#include "CustomAssert.h"
#include "Logger.h"
#include "Security/Enc.h"
#include <fstream>
#include <string>
#include <thread>
std::string ServerName;
std::string ServerDesc;
std::string Resource;
std::string MapName;
std::string Key;
int MaxPlayers;
bool Private;
int MaxCars;
bool Debug;
int Port;

void SetValues(const std::string& Line, int Index) {
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
        Debug = FoundTrue; //checks and sets the Debug Value
        break;
    case 2:
        Private = FoundTrue; //checks and sets the Private Value
        break;
    case 3:
        Port = std::stoi(Data, &sz); //sets the Port
        break;
    case 4:
        MaxCars = std::stoi(Data, &sz); //sets the Max Car amount
        break;
    case 5:
        MaxPlayers = std::stoi(Data, &sz); //sets the Max Amount of player
        break;
    case 6:
        MapName = Data; //Map
        break;
    case 7:
        ServerName = Data; //Name
        break;
    case 8:
        ServerDesc = Data; //desc
        break;
    case 9:
        Resource = Data; //File name
        break;
    case 10:
        Key = Data; //File name
    default:
        break;
    }
}
std::string RemoveComments(const std::string& Line) {
    std::string Return;
    for (char c : Line) {
        if (c == '#')
            break;
        Return += c;
    }
    return Return;
}
void LoadConfig(std::ifstream& IFS) {
    Assert(IFS.is_open());
    std::string line;
    int index = 1;
    while (getline(IFS, line)) {
        index++;
    }
    if (index - 1 < 11) {
        error(("Outdated/Incorrect config please remove it server will close in 5 secs"));
        std::this_thread::sleep_for(std::chrono::seconds(3));
        _Exit(0);
    }
    IFS.close();
    IFS.open(("Server.cfg"));
    info(("Config found updating values"));
    index = 1;
    while (getline(IFS, line)) {
        if (line.rfind('#', 0) != 0 && line.rfind(' ', 0) != 0) { //Checks if it starts as Comment
            std::string CleanLine = RemoveComments(line); //Cleans it from the Comments
            SetValues(CleanLine, index); //sets the values
            index++;
        }
    }
}
void GenerateConfig() {
    std::ofstream FileStream;
    FileStream.open(("Server.cfg"));
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
}
void Default() {
    info(("Config not found generating default"));
    GenerateConfig();
    error(("You are required to input the AuthKey"));
    std::this_thread::sleep_for(std::chrono::seconds(3));
    _Exit(0);
}
void DebugData() {
    debug(std::string("Debug : ") + (Debug ? "true" : "false"));
    debug(std::string("Private : ") + (Private ? "true" : "false"));
    debug("Port : " + std::to_string(Port));
    debug("Max Cars : " + std::to_string(MaxCars));
    debug("MaxPlayers : " + std::to_string(MaxPlayers));
    debug("MapName : " + MapName);
    debug("ServerName : " + ServerName);
    debug("ServerDesc : " + ServerDesc);
    debug("File : " + Resource);
    debug("Key length: " + std::to_string(Key.length()));
}
void InitConfig() {
    ////TODO: Move to json after update 4

    std::ifstream IFS;
    IFS.open(("Server.cfg"));
    if (IFS.good())
        LoadConfig(IFS);
    else
        Default();
    if (IFS.is_open())
        IFS.close();
    if (Key.empty()) {
        error(("No AuthKey was found"));
        std::this_thread::sleep_for(std::chrono::seconds(3));
        _Exit(0);
    }
    if (Debug)
        DebugData();
}
