///
/// Created by Anonymous275 on 28/01/2020
///

#include <iostream>
#include <string>
#include <fstream>
#include <chrono>
#include <thread>
#include "logger.h"
#include <algorithm>
#include "Settings.hpp"

void DebugData();
void LogInit();
void ParseConfig();
void addToLog(const string& Data);
//void ServerMain(int Port, int MaxClients);
void HeartbeatInit();
std::string ServerVersion = "0.3";
std::string ClientVersion = "1.3+";
std::string CustomIP;
void HandleResources(std::string path);
//void TCPMain(int Port);
void NetMain();
//Entry
int main(int argc, char* argv[]) {
    if(argc > 1){
        CustomIP = argv[1];
        size_t n = std::count(CustomIP.begin(), CustomIP.end(), '.');
        int p = CustomIP.find_first_not_of(".0123456789");
        if(p != std::string::npos || n != 3 || CustomIP.substr(0,3) == "127"){
            CustomIP.clear();
            warn("IP Specified is invalid!");
        }else info("Started with custom ip : " + CustomIP);
    }
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
    debug("Auth Key : " + Key);
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
