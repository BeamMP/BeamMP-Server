#pragma once

#include <string>
#include <unordered_map>

namespace Http {
std::string GET(const std::string& host, int port, const std::string& target);
std::string POST(const std::string& host, const std::string& target, const std::unordered_map<std::string, std::string>& fields, const std::string& body, bool json, int* status = nullptr);
}
