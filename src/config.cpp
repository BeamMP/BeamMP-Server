///
/// Created by Anonymous275 on 1/28/2020
///

#include <iostream>
#include <fstream>
#include <string>
#include "logger.h"

void GenerateConfig();
string RemoveComments(const string& Line);
void SetValues(const string& Line, int Index);
string MapName = "/levels/gridmap/info.json";
string ServerName = "BeamMP Server";
string Resource = "Resources";
string Key;
bool Private = false;
bool Debug = false;
int MaxPlayers = 10;
int Port = 30814;
int MaxCars = 1;

//Generates or Reads Config
void ParseConfig(){
    ifstream InFileStream;
    InFileStream.open("Server.cfg");
    if(InFileStream.good()){ //Checks if Config Exists
        info("Config Found Updating Values");
        string line;
        int index = 1;
        while (getline(InFileStream, line)) {
            if(line.rfind('#', 0) != 0){ //Checks if it starts as Comment
                string CleanLine = RemoveComments(line); //Cleans it from the Comments
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



void SetValues(const string& Line, int Index) {
    int i = 0, state = 0;
    char Data[50] = "";
    bool Switch = false;
    if (Index > 5)Switch = true;
    for (char c : Line) {
        if (Switch) {
            if (c == '\"') { state++; }
            if (state > 0 && state < 2) {
                Data[i] = c;
                i++;
            }
        } else {
            if (c == ' ') { state++; }
            if (state > 1) {
                Data[i] = c;
                i++;
            }
        }
    }
    for (int C = 1; C <= i; C++){
        Data[C-1] = Data[C];
    }
    string::size_type sz;
    bool Boolean = std::string(Data).find("true") != string::npos;//searches for "true"
    switch (Index){
        case 1 : Debug = Boolean;//checks and sets the Debug Value
            break;
        case 2 : Private = Boolean;//checks and sets the Private Value
            break;
        case 3 : Port = stoi(Data, &sz);//sets the Port
            break;
        case 4 : MaxCars = stoi(Data, &sz);//sets the Max Car amount
            break;
        case 5 : MaxPlayers = stoi(Data, &sz); //sets the Max Amount of player
            break;
        case 6 : MapName = Data; //Map
            break;
        case 7 : ServerName = Data; //Name
            break;
        case 8 : Resource = Data; //File name
            break;
        case 9 : Key = Data; //File name
    }
}



//generates default Config
void GenerateConfig(){
    ofstream FileStream;
    FileStream.open ("Server.cfg");
    FileStream << "# This is the BeamMP Server Configuration File\n"
                  "Debug = false # true or false to enable debug console output\n"
                  "Private = false # Private?\n"
                  "Port = 30814 # Port to run the server on UDP and TCP\n"
                  "Cars = 1 # Max cars for every player\n"
                  "MaxPlayers = 10 # Maximum Amount of Clients\n"
                  "Map = \"/levels/gridmap/info.json\" # Default Map\n"
                  "Name = \"BeamMP New Server\" # Server Name\n"
                  "use = \"Resources\" # Resource file name\n"
                  "AuthKey = \"\" # Auth Key";
    FileStream.close();
}


string RemoveComments(const string& Line){
    int i = 0;
    char Data[50] = "";
    for(char c : Line) {
        if(c == '#'){break;} //when it finds the # it will stop
        Data[i] = c;
        i++;
    }
    return std::string(Data); //Converts it from a char array to string and returns it
}