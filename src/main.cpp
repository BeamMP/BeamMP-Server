#include "Common.h"
#include "TConfig.h"
#include "THeartbeatThread.h"
#include "TLuaEngine.h"
#include "TPPSMonitor.h"
#include "TResourceManager.h"
#include "TServer.h"
#include "TUDPServer.h"
#include <iostream>
#include <thread>

#ifdef __unix
#include <TTCPServer.h>
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

    bool Shutdown = false;
    Application::RegisterShutdownHandler([&Shutdown] { Shutdown = true; });

    TServer Server(argc, argv);
    [[maybe_unused]] TConfig Config("Server.cfg");
    TResourceManager ResourceManager;
    TPPSMonitor PPSMonitor(Server);
    THeartbeatThread Heartbeat(ResourceManager, Server);
    TTCPServer TCPServer(Server, PPSMonitor, ResourceManager);
    TUDPServer UDPServer(Server, PPSMonitor, TCPServer);
    TCPServer.SetUDPServer(UDPServer);
    TLuaEngine LuaEngine(Server, TCPServer, UDPServer);

    // TODO: replace
    while (!Shutdown) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}
