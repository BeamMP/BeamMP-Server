#include "Http.h"

#include "Client.h"
#include "Common.h"
#include "CustomAssert.h"
#include "LuaAPI.h"
#include "httplib.h"

#include <map>
#include <nlohmann/json.hpp>
#include <random>
#include <stdexcept>

// TODO: Add sentry error handling back

using json = nlohmann::json;

std::string Http::GET(const std::string& host, int port, const std::string& target, unsigned int* status) {
    httplib::SSLClient client(host, port);
    client.enable_server_certificate_verification(false);
    client.set_address_family(AF_INET);
    auto res = client.Get(target.c_str());
    if (res) {
        if (status) {
            *status = res->status;
        }
        return res->body;
    } else {
        return Http::ErrorString;
    }
}

std::string Http::POST(const std::string& host, int port, const std::string& target, const std::string& body, const std::string& ContentType, unsigned int* status, const httplib::Headers& headers) {
    httplib::SSLClient client(host, port);
    client.set_read_timeout(std::chrono::seconds(10));
    beammp_assert(client.is_valid());
    client.enable_server_certificate_verification(false);
    client.set_address_family(AF_INET);
    auto res = client.Post(target.c_str(), headers, body.c_str(), body.size(), ContentType.c_str());
    if (res) {
        if (status) {
            *status = res->status;
        }
        return res->body;
    } else {
        beammp_debug("POST failed: " + httplib::to_string(res.error()));
        return Http::ErrorString;
    }
}

// RFC 2616, RFC 7231
static std::map<size_t, const char*> Map = {
    { -1, "Invalid Response Code" },
    { 100, "Continue" },
    { 101, "Switching Protocols" },
    { 102, "Processing" },
    { 103, "Early Hints" },
    { 200, "OK" },
    { 201, "Created" },
    { 202, "Accepted" },
    { 203, "Non-Authoritative Information" },
    { 204, "No Content" },
    { 205, "Reset Content" },
    { 206, "Partial Content" },
    { 207, "Multi-Status" },
    { 208, "Already Reported" },
    { 226, "IM Used" },
    { 300, "Multiple Choices" },
    { 301, "Moved Permanently" },
    { 302, "Found" },
    { 303, "See Other" },
    { 304, "Not Modified" },
    { 305, "Use Proxy" },
    { 306, "(Unused)" },
    { 307, "Temporary Redirect" },
    { 308, "Permanent Redirect" },
    { 400, "Bad Request" },
    { 401, "Unauthorized" },
    { 402, "Payment Required" },
    { 403, "Forbidden" },
    { 404, "Not Found" },
    { 405, "Method Not Allowed" },
    { 406, "Not Acceptable" },
    { 407, "Proxy Authentication Required" },
    { 408, "Request Timeout" },
    { 409, "Conflict" },
    { 410, "Gone" },
    { 411, "Length Required" },
    { 412, "Precondition Failed" },
    { 413, "Payload Too Large" },
    { 414, "URI Too Long" },
    { 415, "Unsupported Media Type" },
    { 416, "Range Not Satisfiable" },
    { 417, "Expectation Failed" },
    { 421, "Misdirected Request" },
    { 422, "Unprocessable Entity" },
    { 423, "Locked" },
    { 424, "Failed Dependency" },
    { 425, "Too Early" },
    { 426, "Upgrade Required" },
    { 428, "Precondition Required" },
    { 429, "Too Many Requests" },
    { 431, "Request Header Fields Too Large" },
    { 451, "Unavailable For Legal Reasons" },
    { 500, "Internal Server Error" },
    { 501, "Not Implemented" },
    { 502, "Bad Gateway" },
    { 503, "Service Unavailable" },
    { 504, "Gateway Timeout" },
    { 505, "HTTP Version Not Supported" },
    { 506, "Variant Also Negotiates" },
    { 507, "Insufficient Storage" },
    { 508, "Loop Detected" },
    { 510, "Not Extended" },
    { 511, "Network Authentication Required" },
    // cloudflare status codes
    { 520, "(CDN) Web Server Returns An Unknown Error" },
    { 521, "(CDN) Web Server Is Down" },
    { 522, "(CDN) Connection Timed Out" },
    { 523, "(CDN) Origin Is Unreachable" },
    { 524, "(CDN) A Timeout Occurred" },
    { 525, "(CDN) SSL Handshake Failed" },
    { 526, "(CDN) Invalid SSL Certificate" },
    { 527, "(CDN) Railgun Listener To Origin Error" },
    { 530, "(CDN) 1XXX Internal Error" },
};

static const char Magic[] = {
    0x20, 0x2f, 0x5c, 0x5f,
    0x2f, 0x5c, 0x0a, 0x28,
    0x20, 0x6f, 0x2e, 0x6f,
    0x20, 0x29, 0x0a, 0x20,
    0x3e, 0x20, 0x5e, 0x20,
    0x3c, 0x0a, 0x00
};

std::string Http::Status::ToString(int Code) {
    if (Map.find(Code) != Map.end()) {
        return Map.at(Code);
    } else {
        return std::to_string(Code);
    }
}

Http::Server::THttpServerInstance::THttpServerInstance() {
    Application::SetSubsystemStatus("HTTPServer", Application::Status::Starting);
    mThread = std::thread(&Http::Server::THttpServerInstance::operator(), this);
    mThread.detach();
}

#define API_V1 "/v1"

void Http::Server::THttpServerInstance::operator()() try {
    beammp_infof("HTTP Server starting on [{}]:{}", Application::Settings.HTTPServerIP, Application::Settings.HTTPServerPort);
    if (Application::Settings.Private
        && Application::Settings.HTTPServerIP != "::"
        && Application::Settings.HTTPServerIP != "localhost"
        && Application::Settings.HTTPServerIP != "127.0.0.1") {
        beammp_warnf("HTTP Server is running on a possibly public IP ({}), but the server is private. The server's HTTP API supports viewing config, players, and other information, even though the server is private. (this is not an error)", Application::Settings.HTTPServerIP);
    }
    std::unique_ptr<httplib::Server> HttpLibServerInstance = std::make_unique<httplib::Server>();
    // todo: make this IP agnostic so people can set their own IP
    HttpLibServerInstance->Get("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("<!DOCTYPE html><article><h1>Hello World!</h1><section><p>BeamMP Server can now serve HTTP requests!</p></section></article></html>", "text/html");
    });

    HttpLibServerInstance->Get(API_V1 "/players", [](const httplib::Request&, httplib::Response& res) {
        res.status = 200;
        json Players = json::array();
        LuaAPI::MP::Engine->Server().ForEachClient([&](std::weak_ptr<TClient> ClientPtr) {
            Players.push_back(ClientPtr.lock()->GetName());
            return true;
        });
        res.set_content(Players.dump(),
            "application/json");
    });

    HttpLibServerInstance->Get(API_V1 "/config", [](const httplib::Request&, httplib::Response& res) {
        res.status = 200;
        res.set_content(json {
                            { "name", Application::Settings.ServerName },
                            { "description", Application::Settings.ServerDesc },
                            { "max_players", Application::Settings.MaxPlayers },
                            { "max_cars", Application::Settings.MaxCars },
                            { "map", Application::Settings.MapName },
                            { "private", Application::Settings.Private },
                        }
                            .dump(),
            "application/json");
    });

    HttpLibServerInstance->Get(API_V1 "/ready", [](const httplib::Request&, httplib::Response& res) {
        auto Statuses = Application::GetSubsystemStatuses();
        bool Started = true;
        for (const auto& NameStatusPair : Statuses) {
            switch (NameStatusPair.second) {
            case Application::Status::Starting:
            case Application::Status::ShuttingDown:
            case Application::Status::Shutdown:
                Started = false;
                break;
            case Application::Status::Good:
            case Application::Status::Bad:
                break;
            }
        }
        res.status = 200;
        res.set_content(Started ? "true" : "false", "text/plain");
    });

    HttpLibServerInstance->Get(API_V1 "/health", [](const httplib::Request&, httplib::Response& res) {
        size_t SystemsGood = 0;
        size_t SystemsBad = 0;
        json Good = json::array();
        json Bad = json::array();
        auto Statuses = Application::GetSubsystemStatuses();
        for (const auto& NameStatusPair : Statuses) {
            switch (NameStatusPair.second) {
            case Application::Status::Starting:
            case Application::Status::ShuttingDown:
            case Application::Status::Shutdown:
            case Application::Status::Good:
                Good.push_back(NameStatusPair.first);
                SystemsGood++;
                break;
            case Application::Status::Bad:
                Bad.push_back(NameStatusPair.first);
                SystemsBad++;
                break;
            }
        }
        res.set_content(
            json {
                { "healthy", SystemsBad == 0 },
                { "good", Good },
                { "bad", Bad },
            }
                .dump(),
            "application/json");
        res.status = 200;
    });
    // magic endpoint
    HttpLibServerInstance->Get({ 0x2f, 0x6b, 0x69, 0x74, 0x74, 0x79 }, [](const httplib::Request&, httplib::Response& res) {
        res.set_content(std::string(Magic), "text/plain");
    });
    HttpLibServerInstance->set_logger([](const httplib::Request& Req, const httplib::Response& Res) {
        beammp_debug("Http Server: " + Req.method + " " + Req.target + " -> " + std::to_string(Res.status));
    });
    HttpLibServerInstance->set_error_handler([](const httplib::Request&, httplib::Response& Res) {
        if (Res.status >= 400 && Res.body.empty()) {
            Res.set_content("Error: " + std::to_string(Res.status) + " " + Map.at(Res.status), "text/plain");
        }
    });
    HttpLibServerInstance->set_exception_handler([](const httplib::Request& Req, httplib::Response& Res, std::exception& e) {
        Res.status = 500;
        Res.set_content("Internal Server Error", "text/plain");
        beammp_errorf("Exception in http server serving '{}': {}", Req.target, e.what());
    });
    Application::SetSubsystemStatus("HTTPServer", Application::Status::Good);
    auto ret = HttpLibServerInstance->listen(Application::Settings.HTTPServerIP.c_str(), Application::Settings.HTTPServerPort);
    if (!ret) {
        beammp_error("Failed to start http server (failed to listen). Please ensure the http server is configured properly in the ServerConfig.toml, or turn it off if you don't need it.");
    }
} catch (const std::exception& e) {
    beammp_error("Failed to start http server. Please ensure the http server is configured properly in the ServerConfig.toml, or turn it off if you don't need it. Error: " + std::string(e.what()));
}
