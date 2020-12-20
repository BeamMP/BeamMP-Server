// Copyright (c) 2019-present Anonymous275.
// BeamMP Server code is not in the public domain and is not free software.
// One must be granted explicit permission by the copyright holder in order to modify or distribute any part of the source or binaries.
// Anything else is prohibited. Modified works may not be published and have be upstreamed to the official repository.
///
/// Created by Anonymous275 on 4/2/2020.
///
#pragma once

#include <iostream>
#include <mutex>
#include <string>
void InitLog();
#define DebugPrintTID() DebugPrintTIDInternal(__func__, false)
void DebugPrintTIDInternal(const std::string& func, bool overwrite = true); // prints the current thread id in debug mode, to make tracing of crashes and asserts easier
void ConsoleOut(const std::string& msg);
void except(const std::string& toPrint);
void debug(const std::string& toPrint);
void error(const std::string& toPrint);
void info(const std::string& toPrint);
void warn(const std::string& toPrint);
