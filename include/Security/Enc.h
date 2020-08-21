///
/// Created by Anonymous275 on 7/28/2020
///
#pragma once
#include <WS2tcpip.h>
#include <string>
#include "Xor.h"
struct RSA{
    int n = 0;
    int e = 0;
    int d = 0;
};
std::string RSA_E(const std::string& Data, RSA*k);
std::string RSA_D(const std::string& Data, RSA*k);
int Handle(EXCEPTION_POINTERS *ep,char* Origin);
RSA* GenKey();
