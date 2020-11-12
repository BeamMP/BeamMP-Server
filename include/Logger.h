///
/// Created by Anonymous275 on 4/2/2020.
///
#pragma once
#include "Security/Xor.h"
#include <iostream>
#include <mutex>
#include <string>
void InitLog();
#define DebugPrintTID() DebugPrintTIDInternal(__func__, false)
void DebugPrintTIDInternal(const std::string& func, bool overwrite = true); // prints the current thread id in debug mode, to make tracing of crashes and asserts easier
void ConsoleOut(const std::string& msg);
void QueueAbort();
void except(const std::string& toPrint);
void debug(const std::string& toPrint);
void error(const std::string& toPrint);
void info(const std::string& toPrint);
void warn(const std::string& toPrint);
