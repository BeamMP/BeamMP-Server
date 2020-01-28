//
// Created by jojos38 on 28.01.2020.
//

#ifndef NETWORK_H
#define NETWORK_H

#include "enet/enet.h"
#pragma comment(lib,"ws2_32.lib")
#pragma comment(lib,"winmm.lib")
#pragma comment(lib,"enet.lib")

void startRUDP(char host[], int port);

#endif // NETWORK_H
