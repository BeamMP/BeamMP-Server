// Author: lionkor

#pragma once

// This header defines unix equivalents of common win32 functions.

#ifndef WIN32

#include "CustomAssert.h"
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>

// ZeroMemory is just a {0} or a memset(addr, 0, len), and it's a macro on MSVC
inline void ZeroMemory(void* dst, size_t len) {
    Assert(std::memset(dst, 0, len) != nullptr);
}
// provides unix equivalent of closesocket call in win32
inline void closesocket(int socket) {
    shutdown(socket, SHUT_RDWR);
    close(socket);
}

#ifndef __try
#define __try
#endif

#ifndef __except
#define __except(x) /**/
#endif

#endif // WIN32
