#include "SignalHandling.h"
#include "Common.h"

#if defined(BEAMMP_LINUX) || defined(BEAMMP_APPLE)
#include <csignal>
static void UnixSignalHandler(int sig) {
    switch (sig) {
    case SIGPIPE:
        beammp_warn("ignoring SIGPIPE");
        break;
    case SIGTERM:
        beammp_info("gracefully shutting down via SIGTERM");
        Application::GracefullyShutdown();
        break;
    case SIGINT:
        beammp_info("gracefully shutting down via SIGINT");
        Application::GracefullyShutdown();
        break;
    default:
        beammp_debug("unhandled signal: " + std::to_string(sig));
        break;
    }
}
#endif // UNIX

#ifdef BEAMMP_WINDOWS
#include <windows.h>
// return TRUE if handled, FALSE if not
BOOL WINAPI Win32CtrlC_Handler(DWORD CtrlType) {
    switch (CtrlType) {
    case CTRL_C_EVENT:
        beammp_info("gracefully shutting down via CTRL+C");
        Application::GracefullyShutdown();
        return TRUE;
    case CTRL_BREAK_EVENT:
        beammp_info("gracefully shutting down via CTRL+BREAK");
        Application::GracefullyShutdown();
        return TRUE;
    case CTRL_CLOSE_EVENT:
        beammp_info("gracefully shutting down via close");
        Application::GracefullyShutdown();
        return TRUE;
    }
    // we dont care for any others like CTRL_LOGOFF_EVENT and CTRL_SHUTDOWN_EVENT
    return FALSE;
}
#endif // WINDOWS

void SetupSignalHandlers() {
    // signal handlers for unix#include <windows.h>
#if defined(BEAMMP_LINUX) || defined(BEAMMP_APPLE)
    beammp_trace("registering handlers for signals");
    signal(SIGPIPE, UnixSignalHandler);
    signal(SIGTERM, UnixSignalHandler);
#ifndef DEBUG
    signal(SIGINT, UnixSignalHandler);
#endif // DEBUG
#elif defined(BEAMMP_WINDOWS)
    beammp_trace("registering handlers for CTRL_*_EVENTs");
    SetConsoleCtrlHandler(Win32CtrlC_Handler, TRUE);
#endif
}
