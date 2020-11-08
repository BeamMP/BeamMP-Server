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
#include <cstring>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <array>

std::vector<std::string> QConsoleOut;
std::string CInputBuff;
std::mutex MLock;
std::unique_ptr<Lua> LuaConsole;
void HandleInput(const std::string& cmd) {
    std::cout << std::endl;
    if (cmd == "exit") {
        _Exit(0);
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

static struct termios old, current;

void initTermios(int echo) {
    tcgetattr(0, &old); /* grab old terminal i/o settings */
    current = old; /* make new settings same as old settings */
    current.c_lflag &= ~ICANON; /* disable buffered i/o */
    if (echo) {
        current.c_lflag |= ECHO; /* set echo mode */
    } else {
        current.c_lflag &= ~ECHO; /* set no echo mode */
    }
    tcsetattr(0, TCSANOW, &current); /* use these new terminal i/o settings now */
}

void resetTermios(void) {
    tcsetattr(0, TCSANOW, &old);
}

char getch_(int echo) {
    char ch;
    initTermios(echo);
    ch = getchar();
    resetTermios();
    return ch;
}

char _getch(void) {
    return getch_(0);
}

#endif // WIN32

void SetupConsole() {
#if defined(WIN32) && !defined(DEBUG)
    DWORD outMode = 0;
    HANDLE stdoutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (stdoutHandle == INVALID_HANDLE_VALUE) {
        error("Invalid handle");
        std::this_thread::sleep_for(std::chrono::seconds(3));
        _Exit(GetLastError());
    }
    if (!GetConsoleMode(stdoutHandle, &outMode)) {
        error("Invalid console mode");
        std::this_thread::sleep_for(std::chrono::seconds(3));
        _Exit(GetLastError());
    }
    // Enable ANSI escape codes
    outMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (!SetConsoleMode(stdoutHandle, outMode)) {
        error("failed to set console mode");
        std::this_thread::sleep_for(std::chrono::seconds(3));
        _Exit(GetLastError());
    }
#else
#endif // WIN32
}

static std::vector<std::string> ConsoleHistory {};
static size_t ConsoleHistoryReadIndex { 0 };

static inline void ConsoleHistoryAdd(const std::string& cmd) {
    ConsoleHistory.push_back(cmd);
    ConsoleHistoryReadIndex = ConsoleHistory.size();
}
static std::string CompositeInput;
static bool CompositeInputExpected { false };

static void ProcessCompositeInput() {
    if (CompositeInput.size() == 2 && memcmp(CompositeInput.data(), std::array<char, 2> { 91, 65 }.data(), 2) == 0) {
        // UP ARROW
        if (!ConsoleHistory.empty()) {
            if (ConsoleHistoryReadIndex != 0) {
                ConsoleHistoryReadIndex -= 1;
            }
            CInputBuff = ConsoleHistory.at(ConsoleHistoryReadIndex);
        }
    } else if (CompositeInput.size() == 2 && memcmp(CompositeInput.data(), std::array<char, 2> { 91, 66 }.data(), 2) == 0) {
        // DOWN ARROW
        if (!ConsoleHistory.empty()) {
            if (ConsoleHistoryReadIndex != ConsoleHistory.size() - 1) {
                ConsoleHistoryReadIndex += 1;
                CInputBuff = ConsoleHistory.at(ConsoleHistoryReadIndex);
            } else {
                CInputBuff = "";
            }
        }
    } else {
        // not composite input, we made a mistake, so lets just add it to the buffer like usual
        CInputBuff += CompositeInput;
    }
    // ensure history doesnt grow too far beyond a max
    static constexpr size_t MaxHistory = 10;
    if (ConsoleHistory.size() > 2 * MaxHistory) {
        std::vector<std::string> NewHistory(ConsoleHistory.begin() + ConsoleHistory.size() - MaxHistory, ConsoleHistory.end());
        ConsoleHistory = std::move(NewHistory);
        ConsoleHistoryReadIndex = ConsoleHistory.size();
    }
}

[[noreturn]] void ReadCin() {
    DebugPrintTID();
    while (true) {
        int In = _getch();
        //info(std::to_string(In));
        if (CompositeInputExpected) {
            CompositeInput += In;
            if (CompositeInput.size() == 2) {
                CompositeInputExpected = false;
                ProcessCompositeInput();
            }
            continue;
        }
        if (In == 13 || In == '\n') {
            if (!CInputBuff.empty()) {
                HandleInput(CInputBuff);
                ConsoleHistoryAdd(CInputBuff);
                CInputBuff.clear();
            }
        } else if (In == 8 || In == 127) {
            if (!CInputBuff.empty())
                CInputBuff.pop_back();
        } else if (In == 4) {
            CInputBuff = "exit";
            HandleInput(CInputBuff);
            CInputBuff.clear();
        } else if (In == 27) {
            // escape char, assume stuff follows
            CompositeInputExpected = true;
            CompositeInput.clear();
        } else if (!isprint(In)) {
            // ignore
        } else {
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
