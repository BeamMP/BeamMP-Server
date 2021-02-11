#include "CustomAssert.h"

#include "Settings.h"
#include "SocketIO.h"
#include "Startup.h"
#include <iostream>
#include <thread>
#ifndef WIN32
#include <csignal>
void UnixSignalHandler(int sig) {
    switch (sig) {
    case SIGPIPE:
        warn(("ignored signal SIGPIPE: Pipe broken"));
        break;
    default:
        error(("Signal arrived in handler but was not handled: ") + std::to_string(sig));
        break;
    }
}
#endif // WIN32

int main(int argc, char* argv[]) {
#ifndef WIN32
    // ignore SIGPIPE, the signal that is sent for example when a client
    // disconnects while data is being sent to him ("broken pipe").
    signal(SIGPIPE, UnixSignalHandler);
#endif // WIN32
    DebugPrintTID();
    StartTime = std::chrono::high_resolution_clock::now();

    ConsoleInit();
    (void)SocketIO::Get(); // call to be sure it initializes
    InitServer(argc, argv);
    InitConfig();
    InitLua();
    InitRes();
    HBInit();
    StatInit();
    NetMain();
    return 0;
}
