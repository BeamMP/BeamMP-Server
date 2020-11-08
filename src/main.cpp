
#include "CustomAssert.h"
#include <curl/curl.h>
#include "Startup.h"
#include <iostream>
#include <thread>

[[noreturn]] void loop(){
    DebugPrintTID();
    while(true){
        std::cout.flush();
        std::this_thread::sleep_for(std::chrono::milliseconds(600));
    }
}

int main(int argc, char* argv[]) {
    try {
    DebugPrintTID();
    // curl needs to be initialized to properly deallocate its resources later
    Assert(curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK);
    #ifdef DEBUG
        std::thread t1(loop);
        t1.detach();
    #endif
    ConsoleInit();
    InitServer(argc,argv);
    InitConfig();
    InitLua();
    InitRes();
    HBInit();
    StatInit();
    NetMain();
    // clean up curl at the end to be sure
    curl_global_cleanup();
    } catch (const std::exception& e) {
        error(std::string(e.what()));
        throw;
    }
    return 0;
}
