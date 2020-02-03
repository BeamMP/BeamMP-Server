//
// Created by Anonymous275 on 28.01.2020.
//

#include "main.h"
#include <iostream>
#include <string>
#include <fstream>
#include "logger.h"
#include <chrono>
#include <thread>

using namespace std; //nameSpace STD
void DebugData();
void LogInit();
void ParseConfig();
void ServerMain(int Port, int MaxClients);
bool Debug = false;
void addToLog(basic_string<char> Data);
int Port = 30814;
int MaxClients = 10;
string MapName = "levels/gridmap/level.json";
string ServerName = "BeamNG-MP FTW";

//Entry
int main() {
    LogInit();
    ParseConfig();
    if(Debug){ //checks if debug is on
        DebugData(); //Prints Debug Data
    }
    setLoggerLevel("ALL");
    ServerMain(Port, MaxClients);
}


void DebugData(){
    cout << "Debug : true" << "\n";
    cout << "Port : " << Port << "\n";
    cout << "MaxPlayers : " << MaxClients << "\n";
    cout << "MapName : " << MapName << "\n";
    cout << "ServerName : " << ServerName << "\n";
}

void SetMainValues(bool D, int P,int MP,string Name,string serverName){
    Debug = D;
    Port = P;
    MapName = Name;
    ServerName = serverName;
    MaxClients = MP;
}

void LogInit(){
    ofstream LFS;
    LFS.open ("Server.log");
    LFS.close();
}

void addToLog(basic_string<char> Data){
    basic_string<char> LogData = "";
    ifstream InFileStream;
    InFileStream.open("Server.log");
    if(InFileStream.good()){
        string line;
        while (getline(InFileStream, line)) {
            LogData = LogData + line + "\n";
        }
    }
    ofstream LFS;
    LFS.open ("Server.log");
    LFS << LogData;
    LFS << Data.c_str();
    LFS.close();
}