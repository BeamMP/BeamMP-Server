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

string MapName = "levels/gridmap/level.json";
string Private = "false";
int MaxPlayers = 10;
int UDPPort = 30814;
int TCPPort = 0;
string ServerName = "BeamMP Server";
string Resource = "/Resources";
string ServerVersion = "0.1";
string ClientVersion = "0.21";


//Entry
int main() {
    LogInit();
    ParseConfig();
    HeartbeatInit();
    if(Debug){ //checks if debug is on
        DebugData(); //Prints Debug Data
    }
    setLoggerLevel("ALL");
    ServerMain(UDPPort, MaxPlayers);
}


void DebugData(){
    cout << "Debug : true" << "\n";
    cout << "Port : " << UDPPort << "\n";
    cout << "MaxPlayers : " << MaxPlayers << "\n";
    cout << "MapName : " << MapName << "\n";
    cout << "ServerName : " << ServerName << "\n";
    cout << "File : " << Resource << "\n";
}

void SetMainValues(bool D, int P,int MP,string Name,string serverName,string filename){
    Debug = D;
    UDPPort = P;
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
