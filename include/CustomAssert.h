// Author: lionkor

/*
 * Asserts are to be used anywhere where assumptions about state are made
 * implicitly. AssertNotReachable is used where code should never go, like in
 * default switch cases which shouldn't trigger. They make it explicit
 * that a place cannot normally be reached and make it an error if they do.
 */

#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <thread>

#include "Common.h"

static const char* const ANSI_RESET = "\u001b[0m";

static const char* const ANSI_BLACK = "\u001b[30m";
static const char* const ANSI_RED = "\u001b[31m";
static const char* const ANSI_GREEN = "\u001b[32m";
static const char* const ANSI_YELLOW = "\u001b[33m";
static const char* const ANSI_BLUE = "\u001b[34m";
static const char* const ANSI_MAGENTA = "\u001b[35m";
static const char* const ANSI_CYAN = "\u001b[36m";
static const char* const ANSI_WHITE = "\u001b[37m";

static const char* const ANSI_BLACK_BOLD = "\u001b[30;1m";
static const char* const ANSI_RED_BOLD = "\u001b[31;1m";
static const char* const ANSI_GREEN_BOLD = "\u001b[32;1m";
static const char* const ANSI_YELLOW_BOLD = "\u001b[33;1m";
static const char* const ANSI_BLUE_BOLD = "\u001b[34;1m";
static const char* const ANSI_MAGENTA_BOLD = "\u001b[35;1m";
static const char* const ANSI_CYAN_BOLD = "\u001b[36;1m";
static const char* const ANSI_WHITE_BOLD = "\u001b[37;1m";

static const char* const ANSI_BOLD = "\u001b[1m";
static const char* const ANSI_UNDERLINE = "\u001b[4m";

#ifdef DEBUG
#include <iostream>
inline void _assert([[maybe_unused]] const char* file, [[maybe_unused]] const char* function, [[maybe_unused]] unsigned line,
    [[maybe_unused]] const char* condition_string, [[maybe_unused]] bool result) {
    if (!result) {
        std::cout << std::flush << "(debug build) TID "
                  << std::this_thread::get_id() << ": ASSERTION FAILED: at "
                  << file << ":" << line << " \n\t-> in "
                  << function << ", Line " << line << ": \n\t\t-> "
                  << "Failed Condition: " << condition_string << std::endl;
        std::cout << "... terminating ..." << std::endl;
        abort();
    }
}

#define beammp_assert(cond) _assert(__FILE__, __func__, __LINE__, #cond, (cond))
#define beammp_assert_not_reachable() _assert(__FILE__, __func__, __LINE__, "reached unreachable code", false)
#else
#define beammp_assert(cond)                                                            \
    do {                                                                               \
        bool result = (cond);                                                          \
        if (!result) {                                                                 \
            beammp_errorf("Assertion failed in '{}:{}': {}.", __func__, _line, #cond); \
            Sentry.LogAssert(#cond, _file_basename, _line, __func__);                  \
        }                                                                              \
    } while (false)
#define beammp_assert_not_reachable()                                                      \
    do {                                                                                   \
        beammp_errorf("Assertion failed in '{}:{}': Unreachable code reached. This may result in a crash or undefined state of the program.", __func__, _line); \
        Sentry.LogAssert("code is unreachable", std::string(_file_basename).substr(std::string(_file_basename).find_last_of("/\\") + 1), _line, __func__);          \
    } while (false)
#endif
