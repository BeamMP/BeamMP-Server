#include "Client.h"
#include "Common.h"
#include "IThreaded.h"
#include "TConfig.h"
#include "TConsole.h"
#include "TServer.h"
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
    }
}
#endif // __unix

int main(int argc, char** argv) {
#ifdef __unix
    info("registering SIGPIPE and SIGTERM handlers");
    signal(SIGPIPE, UnixSignalHandler);
    signal(SIGTERM, UnixSignalHandler);
    signal(SIGINT, UnixSignalHandler);
#endif // __unix

    TServer Server(argc, argv);
    TConfig Config("Server.cfg");

    auto Client = Server.InsertNewClient();
    if (!Client.expired()) {
        Client.lock()->SetName("Lion");
    } else {
        error("fuckj");
        _Exit(-1);
    }

    Server.ForEachClient([](auto client) -> bool { debug(client.lock()->GetName()); return true; });

    Server.RemoveClient(Client);

    // TODO: replace with blocking heartbeat
    bool Shutdown = false;
    Application::RegisterShutdownHandler([&Shutdown] { Shutdown = true; });
    while (!Shutdown) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}
