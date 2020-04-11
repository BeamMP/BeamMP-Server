///
/// Created by Anonymous275 on 28/01/2020
///

#include <iostream>
#include <string>
#include <fstream>
#include "logger.h"
#include <chrono>
#include <thread>

using namespace std;
void DebugData();
void LogInit();
void ParseConfig();
void ServerMain(int Port, int MaxClients);
bool Debug = false;
void addToLog(basic_string<char> Data);
void HeartbeatInit();

string MapName = "levels/gridmap/level.json";
bool Private = false;
int MaxPlayers = 10;
int UDPPort = 30814;
int TCPPort = 30814;
string ServerName = "BeamMP Server";
string Resource = "Resources";
string ServerVersion = "0.1";
string ClientVersion = "0.21";
void HandleResources(const std::string& path);
void TCPMain(int Port);
//Entry
int main() {
    LogInit();
    ParseConfig();
    HandleResources(Resource);
    HeartbeatInit();
    if(Debug){ //checks if debug is on
        DebugData(); //Prints Debug Data
    }
    setLoggerLevel("ALL");
    std::thread TCPThread(TCPMain,TCPPort);
    TCPThread.detach();
    ServerMain(UDPPort, MaxPlayers);
}


void DebugData(){
    cout << "Debug : true" << endl;
    cout << "Port : " << UDPPort << endl;
    cout << "TCP Port : " << TCPPort << endl;
    cout << "MaxPlayers : " << MaxPlayers << endl;
    cout << "MapName : " << MapName << endl;
    cout << "ServerName : " << ServerName << endl;
    cout << "File : " << Resource << endl;
}

void SetMainValues(bool D, int P, int FP,int MP,string Name,string serverName,string filename){
    Debug = D;
    UDPPort = P;
    TCPPort = FP;
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
