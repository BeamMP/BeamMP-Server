#include "RateLimiter.h"

RateLimiter::RateLimiter() {};

bool RateLimiter::isConnectionAllowed(const std::string& client_address) {
    if (RateLimiter::isIPBlocked(client_address)) {
        return false;
    }
    std::lock_guard<std::mutex> lock(m_connection_mutex);
    auto current_time = std::chrono::high_resolution_clock::now();
    auto& violations = m_connection[client_address];

    // Deleting old violations (older than 5 seconds)
    violations.erase(std::remove_if(violations.begin(), violations.end(),
                         [&](const auto& timestamp) {
                             return std::chrono::duration_cast<std::chrono::seconds>(current_time - timestamp).count() > 5;
                         }),
        violations.end());

    violations.push_back(current_time);

    if (violations.size() >= 4) {
        RateLimiter::blockIP(client_address);
        beammp_errorf("[DoS Protection] Client with the IP: {} surpassed the violation treshhold and is now on the blocked list", client_address);
        return false;
    }

    return true; // We allow the connection
}

void RateLimiter::blockIP(const std::string& client_address) {
    std::ofstream block_file("blocked_ips.txt", std::ios::app);
    if (block_file.is_open()) {
        block_file << client_address << std::endl;
    }
}

bool RateLimiter::isIPBlocked(const std::string& client_address) {
    std::ifstream block_file("blocked_ips.txt");
    std::unordered_set<std::string> blockedIPs;

    if (block_file.is_open()) {
        std::string line;
        while (std::getline(block_file, line)) {
            blockedIPs.insert(line);
        }
    }

    return blockedIPs.contains(client_address);
};