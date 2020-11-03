///
/// Created by Anonymous275 on 4/2/2020.
///
#pragma once
#include <string>
#include <iostream>
void InitLog();
#define DebugPrintTID() DebugPrintTIDInternal(__func__)
void DebugPrintTIDInternal(const std::string& func); // prints the current thread id in debug mode, to make tracing of crashes and asserts easier
void ConsoleOut(const std::string& msg);
void QueueAbort();
void except(const std::string& toPrint);
void debug(const std::string& toPrint);
void error(const std::string& toPrint);
void info(const std::string& toPrint);
void warn(const std::string& toPrint);
