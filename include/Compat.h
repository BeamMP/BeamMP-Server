#pragma once

// ======================= UNIX ========================

#ifdef __unix
#include <arpa/inet.h>
#include <termios.h>
#include <unistd.h>
using SOCKET = int;
using DWORD = unsigned long;
using PDWORD = unsigned long*;
using LPDWORD = unsigned long*;
char _getch(void);
#endif // unix

// ======================= WIN32 =======================

#ifdef WIN32
#include <conio.h>
#include <windows.h>
#endif // WIN32

// ======================= OTHER =======================

#if !defined(WIN32) && !defined(__unix)
#error "OS not supported"
#endif

