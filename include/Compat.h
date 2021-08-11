#pragma once

// ======================= UNIX ========================

#ifdef __unix
#include <arpa/inet.h>
#include <sys/socket.h>
#include <termios.h>
#include <unistd.h>
using SOCKET = int;
using DWORD = unsigned long;
using PDWORD = unsigned long*;
using LPDWORD = unsigned long*;
char _getch();
inline void CloseSocketProper(int TheSocket) {
    shutdown(TheSocket, SHUT_RDWR);
    close(TheSocket);
}
#endif // unix

// ======================= WIN32 =======================

#ifdef WIN32
#include <sentry.h>
#include <ws2tcpip.h>
inline void CloseSocketProper(SOCKET TheSocket) {
    shutdown(TheSocket, SD_BOTH);
    closesocket(TheSocket);
}
#endif // WIN32

// ======================= OTHER =======================

#if !defined(WIN32) && !defined(__unix)
#error "OS not supported"
#endif
