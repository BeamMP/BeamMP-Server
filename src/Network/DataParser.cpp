//
// Created by User on 1/29/2020.
//
#include "enet.h"
#include <stdio.h>
void ParseData(size_t Length, enet_uint8* Data, char* Sender, enet_uint8 ChannelID){ //here we will parse the data
    printf("A packet of length %zu containing \"%s\" was received from \"%s\" on channel %u.\n",
           Length,
           Data,
           Sender,
           ChannelID);
}