///
/// Created by Anonymous275 on 28/01/2020
///

#include <iostream>
#include <string>
#include <fstream>
#include "logger.h"
#include "settings.h"
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
    bool Debug = D;
    int UDPPort = P;
    string MapName = Name;
    string ServerName = serverName;
    int MaxPlayers = MP;
    string Resource = filename;
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