///
/// Created by Anonymous275 on 10/29/2020
///

#include "Lua/LuaSystem.hpp"
#ifdef __WIN32
#include <windows.h>
#include <conio.h>
#else // *nix
typedef unsigned long DWORD, *PDWORD, *LPDWORD;
#endif // __WIN32
#include "Logger.h"
#include <iostream>
#include <string>
#include <thread>
#include <mutex>

std::vector<std::string> QConsoleOut;
std::string CInputBuff;
std::mutex MLock;
Lua* LuaConsole;
void HandleInput(const std::string& cmd){
    if (cmd == "exit") {
        exit(0);
    }else LuaConsole->Execute(cmd);
}

void ProcessOut(){
    static size_t len = 2;
    if(QConsoleOut.empty() && len == CInputBuff.length())return;
    printf("%c[2K\r", 27);
    for(const std::string& msg : QConsoleOut)
        if(!msg.empty())std::cout << msg;
    MLock.lock();
    QConsoleOut.clear();
    MLock.unlock();
    std::cout << "> " << CInputBuff;
    len = CInputBuff.length();
}

void ConsoleOut(const std::string& msg){
    MLock.lock();
    QConsoleOut.emplace_back(msg);
    MLock.unlock();
}

[[noreturn]] void OutputRefresh(){
    while(true){
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        ProcessOut();
    }
}
void SetupConsole(){
#ifdef __WIN32
    DWORD outMode = 0;
    HANDLE stdoutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (stdoutHandle == INVALID_HANDLE_VALUE){
        error("Invalid handle");
        std::this_thread::sleep_for(std::chrono::seconds(3));
        exit(GetLastError());
    }
    if (!GetConsoleMode(stdoutHandle, &outMode)){
        error("Invalid console mode");
        std::this_thread::sleep_for(std::chrono::seconds(3));
        exit(GetLastError());
    }
    // Enable ANSI escape codes
    outMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (!SetConsoleMode(stdoutHandle, outMode)) {
        error("failed to set console mode");
        std::this_thread::sleep_for(std::chrono::seconds(3));
        exit(GetLastError());
    }
#endif // __WIN32
}
[[noreturn]] void ReadCin(){
    while (true){
        int In = getchar();
        if (In == 13) {
            if(!CInputBuff.empty()) {
                HandleInput(CInputBuff);
                CInputBuff.clear();
            }
        }else if(In == 8){
            if(!CInputBuff.empty())CInputBuff.pop_back();
        }else CInputBuff += char(In);
    }
}
void ConsoleInit(){
    SetupConsole();
    LuaConsole = new Lua();
    LuaConsole->Console = true;
    LuaConsole->Init();
    printf("> ");
    std::thread In(ReadCin);
    In.detach();
    std::thread Out(OutputRefresh);
    Out.detach();
}
