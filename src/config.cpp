///
/// Created by Anonymous275 on 1/28/2020
///

#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include "logger.h"

void GenerateConfig();
std::string RemoveComments(const std::string& Line);
void SetValues(const std::string& Line, int Index);
std::string MapName = "/levels/gridmap/info.json";
std::string ServerName = "BeamMP New Server";
std::string ServerDesc = "BeamMP Default Description";
std::string Resource = "Resources";
std::string Key;
bool Private = false;
bool Debug = false;
int MaxPlayers = 10;
int Port = 30814;
int MaxCars = 1;

//Generates or Reads Config
void ParseConfig(){
    std::ifstream InFileStream;
    InFileStream.open("Server.cfg");
    if(InFileStream.good()){ //Checks if Config Exists
        std::string line;
        int index = 1;
        while (getline(InFileStream, line)) {
            index++;
        }
        if(index-1 < 11){
            error("Outdated/Incorrect config please remove it server will close in 5 secs");
            std::this_thread::sleep_for(std::chrono::seconds(5));
            exit(3);
        }
        InFileStream.close();
        InFileStream.open("Server.cfg");
        info("Config Found Updating Values");
        index = 1;
        while (getline(InFileStream, line)) {
            if(line.rfind('#', 0) != 0 && line.rfind(' ', 0) != 0){ //Checks if it starts as Comment
                std::string CleanLine = RemoveComments(line); //Cleans it from the Comments
                SetValues(CleanLine,index); //sets the values
                index++;
            }
        }
    }else{
        info("Config Not Found Generating A new One");
        GenerateConfig();
    }
    InFileStream.close();
}



void SetValues(const std::string& Line, int Index) {
    int state = 0;
    std::string Data;
    bool Switch = false;
    if (Index > 5)Switch = true;
    for (char c : Line) {
        if (Switch) {
            if (c == '\"'){state++;}
            if (state > 0 && state < 2) {
                Data += c;
            }
        } else {
            if (c == ' ') { state++; }
            if (state > 1) {
                Data += c;
            }
        }
    }
    Data = Data.substr(1);
    std::string::size_type sz;
    bool Boolean = std::string(Data).find("true") != std::string::npos;//searches for "true"
    switch (Index){
        case 1 : Debug = Boolean;//checks and sets the Debug Value
            break;
        case 2 : Private = Boolean;//checks and sets the Private Value
            break;
        case 3 : Port = std::stoi(Data, &sz);//sets the Port
            break;
        case 4 : MaxCars = std::stoi(Data, &sz);//sets the Max Car amount
            break;
        case 5 : MaxPlayers = std::stoi(Data, &sz); //sets the Max Amount of player
            break;
        case 6 : MapName = Data; //Map
            break;
        case 7 : ServerName = Data; //Name
            break;
        case 8 : ServerDesc = Data; //desc
            break;
        case 9 : Resource = Data; //File name
            break;
        case 10 : Key = Data; //File name
    }
}



//generates default Config
void GenerateConfig(){
    std::ofstream FileStream;
    FileStream.open ("Server.cfg");
    FileStream << "# This is the BeamMP Server Configuration File\n"
                  "Debug = false # true or false to enable debug console output\n"
                  "Private = false # Private?\n"
                  "Port = 30814 # Port to run the server on UDP and TCP\n"
                  "Cars = 1 # Max cars for every player\n"
                  "MaxPlayers = 10 # Maximum Amount of Clients\n"
                  "Map = \"/levels/gridmap/info.json\" # Default Map\n"
                  "Name = \"BeamMP New Server\" # Server Name\n"
                  "Desc = \"BeamMP Default Description\" # Server Description\n"
                  "use = \"Resources\" # Resource file name\n"
                  "AuthKey = \"\" # Auth Key";
    FileStream.close();
}


std::string RemoveComments(const std::string& Line){
    std::string Return;
    for(char c : Line) {
        if(c == '#'){break;} //when it finds the # it will stop
        Return += c;
    }
    return Return; //Converts it from a char array to string and returns it
}