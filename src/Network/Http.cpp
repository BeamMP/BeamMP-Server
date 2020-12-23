// Copyright (c) 2019-present Anonymous275.
// BeamMP Server code is not in the public domain and is not free software.
// One must be granted explicit permission by the copyright holder in order to modify or distribute any part of the source or binaries.
// Anything else is prohibited. Modified works may not be published and have be upstreamed to the official repository.
///
/// Created by Anonymous275 on 4/9/2020
///

#include "CustomAssert.h"
#include <curl/curl.h>
#include <iostream>
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}
std::string HttpRequest(const std::string& IP, int port) {
    CURL* curl;
    CURLcode res;
    std::string readBuffer;
    curl = curl_easy_init();
    Assert(curl);
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, IP.c_str());
        curl_easy_setopt(curl, CURLOPT_PORT, port);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        if (res != CURLE_OK)
            return "-1";
    }
    return readBuffer;
}

std::string PostHTTP(const std::string& IP, const std::string& Fields, bool json) {
    static auto* header = new curl_slist { (char*)"Content-Type: application/json" };
    static std::mutex Lock;
    std::scoped_lock Guard(Lock);
    CURL* curl;
    CURLcode res;
    std::string readBuffer;
    curl = curl_easy_init();
    Assert(curl);
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, IP.c_str());
        if (json)
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, Fields.size());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, Fields.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        if (res != CURLE_OK)
            return "-1";
    }
    return readBuffer;
}
