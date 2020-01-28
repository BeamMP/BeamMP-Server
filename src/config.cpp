//
// Created by Anonymous275 on 1/28/2020.
//

#include "main.h"
#include <iostream>
#include <fstream>
#include <string>
using namespace std; //nameSpace STD
void GenerateConfig();
string RemoveComments(string Line);
string convertToString(char* a, int size);
void SetValues(string Line, int Index);
void SetMainValues(bool D,int P,string Name,string serverName);
bool D;
int P;
string M;
string S;


//Generates or Reads Config
void ParseConfig(){
    ifstream InFileStream;
    InFileStream.open("Server.cfg");
    if(InFileStream.good()){ //Checks if Config Exists
        cout << "Config Found Updating Values \n\n";
        string line;
        int index = 1;
        while (getline(InFileStream, line)) {
            if(line.rfind('#', 0) != 0){ //Checks if it starts as Comment
                string CleanLine = RemoveComments(line); //Cleans it from the Comments
                SetValues(CleanLine,index); //sets the values
                index++;
            }
        }
        SetMainValues(D,P,M,S); //gives the values to Main
    }else{
        cout << "Config Not Found Generating A new One \n";
        GenerateConfig();
    }
    InFileStream.close();
}



void SetValues(string Line, int Index) {
    int i = 0, state = 0;
    char Data[50] = "";
    bool Switch = false;
    if (Index > 2) { Switch = true; }
    for (char &c : Line) {
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
            if(Boolean){D = true;}else{D = false;}//checks and sets the Debug Value
            break;
        case 2 : P = stoi(Data, &sz);//sets the Port
            break;
        case 3 : M = Data; //Map
            break;
        case 4 : S = Data; //Name
    }
}



//generates default Config
void GenerateConfig(){
    ofstream FileStream;
    FileStream.open ("Server.cfg");
    FileStream << "# This is the BeamNG-MP Server Configuration File\n"
                  "Debug = false # true or false to enable debug console output\n"
                  "Port = 30813 # Port to run the server on\n"
                  "Map = \"levels/gridmap/level.json\"\n"
                  "Name = \"BeamNG-MP FTW\"";
    FileStream.close();
}


string RemoveComments(string Line){
    int i = 0;
    char Data[50] = "";
    for(char& c : Line) {
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
    string s = "";
    for (i = 0; i < size; i++) {
        s = s + a[i];
    }
    return s;
}