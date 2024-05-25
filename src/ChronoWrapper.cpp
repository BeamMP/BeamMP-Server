#include "ChronoWrapper.h"
#include "Common.h"
#include <regex>

std::chrono::high_resolution_clock::duration ChronoWrapper::TimeFromStringWithLiteral(const std::string time_str)
{
    // const std::regex time_regex(R"((\d+)(min|ms|us|ns|[dhs]))"); //i.e one of: "25ns, 6us, 256ms, 2s, 13min, 69h, 356d" will get matched (only available in newer C++ versions)
    const std::regex time_regex(R"((\d+\.*\d*)(min|[dhs]))"); //i.e one of: "2.01s, 13min, 69h, 356.69d" will get matched
    std::smatch match;
    int64_t time_value; //TODO: make time_value a float and figure out how to return that as the correct amount of nanoseconds in high_precision_clock::duration
    if (!std::regex_search(time_str, match, time_regex)) return std::chrono::nanoseconds(0);
    time_value = stoi(match.str(1));
    beammp_debugf("Parsed time was: {}{}", time_value, match.str(2));
    if (match.str(2) == "d") {
        return std::chrono::days(time_value);
    }
    else if (match.str(2) == "h") {
        return std::chrono::hours(time_value);
    }
    else if (match.str(2) == "min") {
         return std::chrono::minutes(time_value);
    }
    else if (match.str(2) == "s") {
         return std::chrono::seconds(time_value);
    }
    // else if (match.str(2) == "ms") {
    //      return std::chrono::milliseconds(time_value);
    // }
    // else if (match.str(2) == "us") {
    //      return std::chrono::microseconds(time_value);
    // }
    // else if (match.str(2) == "ns") {
    //      return std::chrono::nanoseconds(time_value);
    // }
    return std::chrono::nanoseconds(0);
}
