///
/// Created by Anonymous275 on 1/28/2020
///

#include <iostream>
#include <fstream>
#include <string>
#include "logger.h"
using namespace std; //nameSpace STD
void GenerateConfig();
string RemoveComments(const string& Line);
string convertToString(char* a, int size);
void SetValues(const string& Line, int Index);
void SetMainValues(bool,int,int,int,string,string,string);
bool D;
int P,FP,MP;
string M,S,F;

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
        SetMainValues(D,P,FP,MP,M,S,F); //gives the values to Main
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
    if (Index > 4) { Switch = true; }
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
    bool Boolean = (convertToString(Data,i-1).find("true") != string::npos);//searches for "true"
    switch (Index){
        case 1 :
            D = Boolean;//checks and sets the Debug Value
            break;
        case 2 : P = stoi(Data, &sz);//sets the Port
            break;
        case 3 : FP = stoi(Data, &sz);//sets the TCP File Port
            break;
        case 4 : MP = stoi(Data, &sz); //sets the Max Amount of player
            break;
        case 5 : M = Data; //Map
            break;
        case 6 : S = Data; //Name
        case 7 : F = Data; //File name
    }
}



//generates default Config
void GenerateConfig(){
    ofstream FileStream;
    FileStream.open ("Server.cfg");
    FileStream << "# This is the BeamMP Server Configuration File\n"
                  "Debug = false # true or false to enable debug console output\n"
                  "Port = 30814 # Port to run the server on\n"
                  "FilePort = 30814 # Port to transfer Files\n"
                  "MaxPlayers = 10 # Maximum Amount of Clients\n"
                  "Map = \"/levels/gridmap/info.json\" # Default Map\n"
                  "Name = \"BeamMP New Server\" # Server Name\n"
                  "use = \"Resources\" # Resource file name";
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
    return convertToString(Data,i); //Converts it from a char array to string and returns it
}

//Converts a char array or pointer to string
string convertToString(char* a, int size)
{
    int i;
    string s;
    for (i = 0; i < size; i++) {
        s = s + a[i];
    }
    return s;
}