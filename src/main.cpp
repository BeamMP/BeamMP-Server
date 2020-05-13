///
/// Created by Anonymous275 on 28/01/2020
///

#include <iostream>
#include <string>
#include <fstream>
#include <chrono>
#include <thread>
#include "logger.h"
#include "Settings.hpp"

using namespace std;
void DebugData();
void LogInit();
void ParseConfig();
void addToLog(const string& Data);
//void ServerMain(int Port, int MaxClients);
void HeartbeatInit();
string ServerVersion = "0.1";
string ClientVersion = "1.1";
void HandleResources(const std::string& path);
//void TCPMain(int Port);
void NetMain();
//Entry
int main() {
    LogInit();
    ParseConfig();
    info("BeamMP Server Running version " + ServerVersion);
    HandleResources(Resource);
    HeartbeatInit();
    if(Debug)DebugData();
    setLoggerLevel(0); //0 for all
    /*std::thread TCPThread(TCPMain,Port);
    TCPThread.detach();*/
    //ServerMain(Port, MaxPlayers);
    NetMain();
}


void DebugData(){
    debug(string("Debug : ") + (Debug?"true":"false"));
    debug(string("Private : ") + (Private?"true":"false"));
    debug("Port : " + to_string(Port));
    debug("Max Cars : " + to_string(MaxCars));
    debug("MaxPlayers : " + to_string(MaxPlayers));
    debug("MapName : " + MapName);
    debug("ServerName : " + ServerName );
    debug("File : " + Resource);
}


void LogInit(){
    ofstream LFS;
    LFS.open ("Server.log");
    LFS.close();
}

void addToLog(const string& Data){
    ofstream LFS;
    LFS.open ("Server.log", std::ios_base::app);
    LFS << Data.c_str();
    LFS.close();
}
