#pragma once

#include <string>
#include <unordered_map>

namespace Http {
std::string GET(const std::string& host, int port, const std::string& target, unsigned int* status = nullptr);
std::string POST(const std::string& host, int port, const std::string& target, const std::string& body, const std::string& ContentType, unsigned int* status = nullptr);
namespace Status {
    std::string ToString(int code);
}
const std::string ErrorString = "-1";
}
