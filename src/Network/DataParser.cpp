//
// Created by Anonymous275 on 4/2/2020.
//
#include "enet.h"
#include <stdio.h>
#include "../logger.h"
using namespace std;
char Name[20] = "";
void ParseData(ENetPacket*packet,ENetPeer*peer){ //here we will parse the data
    enet_uint8* Data = packet->data;
    if(strcmp((char*)peer->data,"Client information")==0){ //Checks if the Client has no name
        sprintf(Name,"%s",Data);
        peer->data = (void *)Name;
        char Info[100];
        sprintf(Info,"Client Name is %s ID : %u\n",Name,peer->connectID); //ID System
        info(Info); //Logs the data
    }
}