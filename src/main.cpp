//
// Created by Anonymous275 on 28.01.2020.
//

#include "main.h"
#include <iostream>
#include <string>
#include "logger.h"

using namespace std; //nameSpace STD

void DebugData();
void ParseConfig();
void ServerMain(int Port, int MaxClients);

bool Debug = false;
int Port = 30814;
int MaxClients = 10;
string MapName = "levels/gridmap/level.json";
string ServerName = "BeamNG-MP FTW";

//Entry
int main() {
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

