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
inline void CloseSocketProper(int socket) {
    shutdown(socket, SHUT_RDWR);
    close(socket);
}
#endif // unix

// ======================= WIN32 =======================

#ifdef WIN32
#include <conio.h>
#include <windows.h>
inline void CloseSocketProper(int socket) {
    shutdown(socket, SHUT_RDWR);
    close(socket);
}
#endif // WIN32

// ======================= OTHER =======================

#if !defined(WIN32) && !defined(__unix)
#error "OS not supported"
#endif
