#pragma once

// Unix - Win32 compatibility stuff
#ifdef WIN32
#include <conio.h>
#include <windows.h>
#else // *nix
typedef unsigned long DWORD, *PDWORD, *LPDWORD;
#include <termios.h>
#include <unistd.h>
#endif // WIN32

#ifndef WIN32

char _getch(void);

#endif // !WIN32
