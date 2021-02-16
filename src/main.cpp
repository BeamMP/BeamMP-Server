#include "Client.h"
#include "Common.h"
#include "IThreaded.h"
#include "TConfig.h"
#include "TConsole.h"
#include "THeartbeatThread.h"
#include "TLuaEngine.h"
#include "TResourceManager.h"
#include "TServer.h"
#include "TPPSMonitor.h"
#include "TUDPServer.h"
#include <atomic>
#include <functional>
#include <iostream>
#include <thread>

#ifdef __unix
#include <csignal>

void UnixSignalHandler(int sig) {
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

int main(int argc, char** argv) {
#ifdef __unix
    info("registering handlers for SIGINT, SIGTERM, SIGPIPE");
    signal(SIGPIPE, UnixSignalHandler);
    signal(SIGTERM, UnixSignalHandler);
    signal(SIGINT, UnixSignalHandler);
#endif // __unix

    TServer Server(argc, argv);
    [[maybe_unused]] TConfig Config("Server.cfg");
    TResourceManager ResourceManager;
    [[maybe_unused]] TPPSMonitor PPSMonitor(Server);
    THeartbeatThread Heartbeat(ResourceManager, Server);
    TTCPServer TCPServer(Server);
    TUDPServer UDPServer(Server, PPSMonitor);
    TLuaEngine LuaEngine(Server);

    // TODO: replace
    bool Shutdown = false;
    Application::RegisterShutdownHandler([&Shutdown] { Shutdown = true; });
    while (!Shutdown) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}
