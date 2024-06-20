#include "Common.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

class RateLimiter {
public:
    RateLimiter();
    bool isConnectionAllowed(const std::string& client_address);

private:
    std::unordered_map<std::string, std::vector<std::chrono::time_point<std::chrono::high_resolution_clock>>> m_connection;
    std::mutex m_connection_mutex;

    void blockIP(const std::string& client_address);
    bool isIPBlocked(const std::string& client_address);
};