///
/// Created by Anonymous275 on 10/29/2020
///

#include "Lua/LuaSystem.hpp"
#ifdef WIN32
#include <conio.h>
#include <windows.h>
#else // *nix
typedef unsigned long DWORD, *PDWORD, *LPDWORD;
#include <termios.h>
#include <unistd.h>
#endif // WIN32
#include "Logger.h"
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

std::vector<std::string> QConsoleOut;
std::string CInputBuff;
std::mutex MLock;
std::unique_ptr<Lua> LuaConsole;
void HandleInput(const std::string& cmd) {
    std::cout << std::endl;
    if (cmd == "exit") {
        exit(0);
    } else
        LuaConsole->Execute(cmd);
}

void ProcessOut() {
    static size_t len = 2;
    if (QConsoleOut.empty() && len == CInputBuff.length())
        return;
    printf("%c[2K\r", 27);
    for (const std::string& msg : QConsoleOut)
        if (!msg.empty())
            std::cout << msg;
    MLock.lock();
    QConsoleOut.clear();
    MLock.unlock();
    std::cout << "> " << CInputBuff << std::flush;
    len = CInputBuff.length();
}

void ConsoleOut(const std::string& msg) {
    MLock.lock();
    QConsoleOut.emplace_back(msg);
    MLock.unlock();
}

[[noreturn]] void OutputRefresh() {
    DebugPrintTID();
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        ProcessOut();
    }
}

#ifndef WIN32
static int _getch() {
    char buf = 0;
    struct termios old;
    fflush(stdout);
    if (tcgetattr(0, &old) < 0)
        perror("tcsetattr()");
    old.c_lflag &= ~unsigned(ICANON);
    old.c_lflag &= ~unsigned(ECHO);
    old.c_cc[VMIN] = 1;
    old.c_cc[VTIME] = 0;
    if (tcsetattr(0, TCSANOW, &old) < 0)
        perror("tcsetattr ICANON");
    if (read(0, &buf, 1) < 0)
        perror("read()");
    old.c_lflag |= ICANON;
    old.c_lflag |= ECHO;
    if (tcsetattr(0, TCSADRAIN, &old) < 0)
        perror("tcsetattr ~ICANON");
    // no echo printf("%c\n", buf);
    return buf;
}
#endif // WIN32

void SetupConsole() {
#if defined(WIN32) && !defined(DEBUG)
    DWORD outMode = 0;
    HANDLE stdoutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (stdoutHandle == INVALID_HANDLE_VALUE) {
        error("Invalid handle");
        std::this_thread::sleep_for(std::chrono::seconds(3));
        exit(GetLastError());
    }
    if (!GetConsoleMode(stdoutHandle, &outMode)) {
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
#else
#endif // WIN32
}

[[noreturn]] void ReadCin() {
    DebugPrintTID();
    while (true) {
        int In = _getch();
        if (In == 13 || In == '\n') {
            if (!CInputBuff.empty()) {
                HandleInput(CInputBuff);
                CInputBuff.clear();
            }
        } else if (In == 8 || In == 127) {
            if (!CInputBuff.empty())
                CInputBuff.pop_back();
        } else if (In == 4) {
            CInputBuff = "exit";
            HandleInput(CInputBuff);
            CInputBuff.clear();
        } else if (!isprint(In)) {
            // ignore
        } else {
            // info(std::to_string(In));
            CInputBuff += char(In);
        }
    }
}
void ConsoleInit() {
    SetupConsole();
    LuaConsole = std::make_unique<Lua>(true);
    printf("> ");
    std::thread In(ReadCin);
    In.detach();
    std::thread Out(OutputRefresh);
    Out.detach();
}
