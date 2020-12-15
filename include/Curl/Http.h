///
/// Created by Anonymous275 on 7/18/2020
///
#pragma once
#include <string>
std::string HttpRequest(const std::string& IP, int port);
std::string PostHTTP(const std::string& IP, const std::string& Fields, bool json);