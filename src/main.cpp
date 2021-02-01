#include "CustomAssert.h"

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

[[noreturn]] void loop() {
    DebugPrintTID();
    while (true) {
        std::cout.flush();
        std::this_thread::sleep_for(std::chrono::milliseconds(600));
    }
}

int main(int argc, char* argv[]) {
#ifndef WIN32
    // ignore SIGPIPE, the signal that is sent for example when a client
    // disconnects while data is being sent to him ("broken pipe").
    signal(SIGPIPE, UnixSignalHandler);
#endif // WIN32
    DebugPrintTID();
#ifdef DEBUG
    std::thread t1(loop);
    t1.detach();
#endif
    ConsoleInit();
    InitServer(argc, argv);
    InitConfig();
    InitLua();
    InitRes();
    HBInit();
    StatInit();
    NetMain();
    return 0;
}
