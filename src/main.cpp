#include "CustomAssert.h"

#include "Startup.h"
#include <curl/curl.h>
#include <iostream>
#include <thread>
#ifndef WIN32
#include <signal.h>
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
    // curl needs to be initialized to properly deallocate its resources later
    [[maybe_unused]] auto ret = curl_global_init(CURL_GLOBAL_DEFAULT);
    Assert(ret == CURLE_OK);
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
    // clean up curl at the end to be sure
    curl_global_cleanup();
    return 0;
}
