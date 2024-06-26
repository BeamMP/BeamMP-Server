#pragma once

#include <chrono>
#include <string>

namespace ChronoWrapper {
std::chrono::high_resolution_clock::duration TimeFromStringWithLiteral(const std::string& time_str);
}
