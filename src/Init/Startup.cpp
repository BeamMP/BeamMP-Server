// Copyright (c) 2019-present Anonymous275.
// BeamMP Server code is not in the public domain and is not free software.
// One must be granted explicit permission by the copyright holder in order to modify or distribute any part of the source or binaries.
// Anything else is prohibited. Modified works may not be published and have be upstreamed to the official repository.
///
/// Created by Anonymous275 on 7/28/2020
///
#include "Client.hpp"
#include "Logger.h"
#include "Security/Enc.h"
#include <algorithm>
#include <string>

std::string CustomIP;
std::string GetSVer() {
    return "1.20";
}
std::string GetCVer() {
    return "1.80";
}
void Args(int argc, char* argv[]) {
    info("BeamMP Server Running version " + GetSVer());
    if (argc > 1) {
        CustomIP = argv[1];
        size_t n = std::count(CustomIP.begin(), CustomIP.end(), '.');
        auto p = CustomIP.find_first_not_of((".0123456789"));
        if (p != std::string::npos || n != 3 || CustomIP.substr(0, 3) == ("127")) {
            CustomIP.clear();
            warn("IP Specified is invalid! Ignoring");
        } else info("Server started with custom IP");
    }
}
void InitServer(int argc, char* argv[]) {
    InitLog();
    Args(argc, argv);
    CI = std::make_unique<ClientInterface>();
}
