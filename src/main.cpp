///
/// Created by Anonymous275 on 28/01/2020
///

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
void HeartbeatInit();
static int Port = 30814;
static int MaxPlayers = 10;
static string MapName = "levels/gridmap/level.json";
static string ServerName = "BeamNG-MP FTW";
static string Resource = "/Resources";
static string ServerVersion = "0.1";
//Entry
int main() {
    LogInit();
    HeartbeatInit();
    ParseConfig();
    if(Debug){ //checks if debug is on
        DebugData(); //Prints Debug Data
    }
    setLoggerLevel("ALL");
    ServerMain(Port, MaxPlayers);
}


void DebugData(){
    cout << "Debug : true" << "\n";
    cout << "Port : " << Port << "\n";
    cout << "MaxPlayers : " << MaxPlayers << "\n";
    cout << "MapName : " << MapName << "\n";
    cout << "ServerName : " << ServerName << "\n";
    cout << "File : " << Resource << "\n";
}

void SetMainValues(bool D, int P,int MP,string Name,string serverName,string filename){
    Debug = D;
    Port = P;
    MapName = Name;
    ServerName = serverName;
    MaxPlayers = MP;
    Resource = filename;
}

void LogInit(){
    ofstream LFS;
    LFS.open ("Server.log");
    LFS.close();
}

void addToLog(basic_string<char> Data){
    ofstream LFS;
    LFS.open ("Server.log", std::ios_base::app);
    LFS << Data.c_str();
    LFS.close();
}