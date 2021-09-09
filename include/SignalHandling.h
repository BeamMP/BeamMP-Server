#pragma once

#include "Common.h"

#ifdef __unix
#include <csignal>
static void UnixSignalHandler(int sig) {
    switch (sig) {
    case SIGPIPE:
        warn("ignoring SIGPIPE");
        break;
    case SIGTERM:
        info("gracefully shutting down via SIGTERM");
        Application::GracefullyShutdown();
        break;
    case SIGINT:
        info("gracefully shutting down via SIGINT");
        Application::GracefullyShutdown();
        break;
    default:
        debug("unhandled signal: " + std::to_string(sig));
        break;
    }
}
#endif // __unix

#ifdef WIN32
#include <windows.h>
// return TRUE if handled, FALSE if not
BOOL WINAPI Win32CtrlC_Handler(DWORD CtrlType) {
    switch (CtrlType) {
    case CTRL_C_EVENT:
        info("gracefully shutting down via CTRL+C");
        Application::GracefullyShutdown();
        return TRUE;
    case CTRL_BREAK_EVENT:
        info("gracefully shutting down via CTRL+BREAK");
        Application::GracefullyShutdown();
        return TRUE;
    case CTRL_CLOSE_EVENT:
        info("gracefully shutting down via close");
        Application::GracefullyShutdown();
        return TRUE;
    }
    // we dont care for any others like CTRL_LOGOFF_EVENT and CTRL_SHUTDOWN_EVENT
    return FALSE;
}
#endif // WIN32

// clang-format off
static void SetupSignalHandlers() {
    // signal handlers for unix
    #ifdef __unix
        trace("registering handlers for SIGINT, SIGTERM, SIGPIPE");
        signal(SIGPIPE, UnixSignalHandler);
        signal(SIGTERM, UnixSignalHandler);
        #ifndef DEBUG
            signal(SIGINT, UnixSignalHandler);
        #endif // DEBUG
    #endif // __unix

        // signal handlers for win32
    #ifdef WIN32
        trace("registering handlers for CTRL_*_EVENTs");
        SetConsoleCtrlHandler(Win32CtrlC_Handler, TRUE);
    #endif // WIN32
}
// clang-format on
