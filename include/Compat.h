#pragma once

#include "Environment.h"

// ======================= UNIX ========================

#ifdef BEAMMP_LINUX
#include <errno.h>
#include <termios.h>
#include <unistd.h>
char _getch();
#endif // unix

// ======================= APPLE ========================

#ifdef BEAMMP_APPLE
#include <errno.h>
#include <termios.h>
#include <unistd.h>
char _getch();
#endif // unix

// ======================= WINDOWS =======================

#ifdef BEAMMP_WINDOWS
#include <conio.h>
#endif // WIN32
