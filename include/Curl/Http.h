// Copyright (c) 2019-present Anonymous275.
// BeamMP Server code is not in the public domain and is not free software.
// One must be granted explicit permission by the copyright holder in order to modify or distribute any part of the source or binaries.
// Anything else is prohibited. Modified works may not be published and have be upstreamed to the official repository.
///
/// Created by Anonymous275 on 7/18/2020
///
#pragma once
#include <string>
#include <unordered_map>
std::string HttpRequest(const std::string& host, int port, const std::string& target);
std::string PostHTTP(const std::string& host, const std::string& target, const std::unordered_map<std::string, std::string>& fields, const std::string& body, bool json);
