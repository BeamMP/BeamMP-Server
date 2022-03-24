#pragma once

#include "Environment.h"

// ======================= UNIX ========================

#ifdef BEAMMP_LINUX
#include <arpa/inet.h>
#include <errno.h>
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

// ======================= APPLE ========================

#ifdef BEAMMP_APPLE
#include <arpa/inet.h>
#include <errno.h>
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

// ======================= WINDOWS =======================

#ifdef BEAMMP_WINDOWS
#include <conio.h>
#include <winsock2.h>
inline void CloseSocketProper(SOCKET TheSocket) {
    shutdown(TheSocket, 2); // 2 == SD_BOTH
    closesocket(TheSocket);
}
#endif // WIN32

#ifdef INVALID_SOCKET
static inline constexpr int BEAMMP_INVALID_SOCKET = INVALID_SOCKET;
#else
static inline constexpr int BEAMMP_INVALID_SOCKET = -1;
#endif
