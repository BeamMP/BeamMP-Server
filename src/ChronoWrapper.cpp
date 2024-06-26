#include "ChronoWrapper.h"
#include "Common.h"
#include <regex>

std::chrono::high_resolution_clock::duration ChronoWrapper::TimeFromStringWithLiteral(const std::string& time_str)
{
    // const std::regex time_regex(R"((\d+\.{0,1}\d*)(min|ms|us|ns|[dhs]))"); //i.e one of: "25ns, 6us, 256ms, 2s, 13min, 69h, 356d" will get matched (only available in newer C++ versions)
    const std::regex time_regex(R"((\d+\.{0,1}\d*)(min|[dhs]))"); //i.e one of: "2.01s, 13min, 69h, 356.69d" will get matched
    std::smatch match;
    float time_value;
    if (!std::regex_search(time_str, match, time_regex)) return std::chrono::nanoseconds(0);
    time_value = stof(match.str(1));
    if (match.str(2) == "d") {
        return std::chrono::seconds((uint64_t)(time_value * 86400)); //86400 seconds in a day
    }
    else if (match.str(2) == "h") {
        return std::chrono::seconds((uint64_t)(time_value * 3600)); //3600 seconds in an hour
    }
    else if (match.str(2) == "min") {
        return std::chrono::seconds((uint64_t)(time_value * 60));
    }
    else if (match.str(2) == "s") {
        return std::chrono::seconds((uint64_t)time_value);
    }
    return std::chrono::nanoseconds(0);
}
