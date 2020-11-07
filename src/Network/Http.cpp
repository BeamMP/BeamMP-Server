///
/// Created by Anonymous275 on 4/9/2020
///

#include <curl/curl.h>
#include "CustomAssert.h"
#include <iostream>
static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp){
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}
std::string HttpRequest(const std::string& IP,int port){
    CURL *curl;
    CURLcode res;
    std::string readBuffer;
    curl = curl_easy_init();
    Assert(curl);
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, IP.c_str());
        curl_easy_setopt(curl, CURLOPT_PORT, port);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        if(res != CURLE_OK)return "-1";
    }
    return readBuffer;
}

std::string PostHTTP(const std::string& IP,const std::string& Fields){
    CURL *curl;
    CURLcode res;
    std::string readBuffer;
    curl = curl_easy_init();
    Assert(curl);
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, IP.c_str());
        /*curl_easy_setopt(curl, CURLOPT_URL, "https://95.216.35.232/heartbeatv2");
        curl_easy_setopt(curl, CURLOPT_PORT, 3600);*/
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, Fields.size());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, Fields.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        if(res != CURLE_OK)return "-1";
    }
    return readBuffer;
}
