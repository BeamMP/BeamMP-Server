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

#include "Logger.h"

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

inline void _assert([[maybe_unused]] const char* file, [[maybe_unused]] const char* function, [[maybe_unused]] unsigned line,
    [[maybe_unused]] const char* condition_string, [[maybe_unused]] bool result) {
    if (!result) {
#if DEBUG
        std::stringstream ss;
        ss << std::this_thread::get_id();
        fprintf(stdout,
            "(debug build) TID %s: %sASSERTION FAILED%s at %s%s:%u%s in \n\t-> in %s%s%s, Line %u: \n\t\t-> "
            "Failed Condition: %s%s%s\n",
            ss.str().c_str(), ANSI_RED_BOLD, ANSI_RESET, ANSI_UNDERLINE, file, line, ANSI_RESET,
            ANSI_BOLD, function, ANSI_RESET, line, ANSI_RED, condition_string,
            ANSI_RESET);
        fprintf(stdout, "%s... terminating with SIGABRT ...%s\n", ANSI_BOLD, ANSI_RESET);
        abort();
#else
        char buf[2048];
        sprintf(buf,
            "%s=> ASSERTION `%s` FAILED IN RELEASE BUILD%s%s -> IGNORING FAILED ASSERTION "
            "& HOPING IT WON'T CRASH%s",
            ANSI_RED_BOLD, condition_string, ANSI_RESET, ANSI_RED, ANSI_RESET);
        error(buf);
#endif
    }
}

#ifndef ASSERT
#define Assert(cond) _assert(__FILE__, __func__, __LINE__, #cond, (cond))
#endif // ASSERT
#define AssertNotReachable() _assert(__FILE__, __func__, __LINE__, "reached unreachable code", false)
