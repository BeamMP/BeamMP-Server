#include "Http.h"

#include "Client.h"
#include "Common.h"
#include "CustomAssert.h"
#include "LuaAPI.h"
#include "httplib.h"

#include <map>
#include <random>
#include <rapidjson/document.h>
#include <rapidjson/rapidjson.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <stdexcept>
fs::path Http::Server::THttpServerInstance::KeyFilePath;
fs::path Http::Server::THttpServerInstance::CertFilePath;
// TODO: Add sentry error handling back

namespace json = rapidjson;

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

long Http::Server::Tx509KeypairGenerator::GenerateRandomId() {
    std::random_device R;
    std::default_random_engine E1(R());
    std::uniform_int_distribution<long> UniformDist(0, ULONG_MAX);
    return UniformDist(E1);
}

// Http::Server::THttpServerInstance::THttpServerInstance() { }
EVP_PKEY* Http::Server::Tx509KeypairGenerator::GenerateKey() {
    /**
     * Allocate memory for the pkey
     */
    EVP_PKEY* PKey = EVP_PKEY_new();
    if (PKey == nullptr) {
        beammp_error("Could not allocate memory for X.509 private key (PKEY) generation.");
        throw std::runtime_error { std::string { "X.509 PKEY allocation error" } };
    }
    BIGNUM* E = BN_new();
    beammp_assert(E); // TODO: replace all these asserts with beammp_errors
    unsigned char three = 3;
    BIGNUM* EErr = BN_bin2bn(&three, sizeof(three), E);
    beammp_assert(EErr);
    RSA* Rsa = RSA_new();
    beammp_assert(Rsa);
    int Ret = RSA_generate_key_ex(Rsa, Crypto::RSA_DEFAULT_KEYLENGTH, E, nullptr);
    beammp_assert(Ret == 1);
    BN_free(E);
    if (!EVP_PKEY_assign_RSA(PKey, Rsa)) {
        EVP_PKEY_free(PKey);
        beammp_error(std::string("Could not generate " + std::to_string(Crypto::RSA_DEFAULT_KEYLENGTH) + "-bit RSA key."));
        throw std::runtime_error { std::string("X.509 RSA key generation error") };
    }
    // todo: figure out if returning by reference instead of passing pointers is a security breach
    return PKey;
}

X509* Http::Server::Tx509KeypairGenerator::GenerateCertificate(EVP_PKEY& PKey) {
    X509* X509 = X509_new();
    if (X509 == nullptr) {
        X509_free(X509);
        beammp_error("Could not allocate memory for X.509 certificate generation.");
        throw std::runtime_error { std::string("X.509 certificate generation error") };
    }

    /**Set the metadata of the certificate*/
    ASN1_INTEGER_set(X509_get_serialNumber(X509), GenerateRandomId());

    /**Set the cert validity to a year*/
    X509_gmtime_adj(X509_get_notBefore(X509), 0);
    X509_gmtime_adj(X509_get_notAfter(X509), 31536000L);

    /**Set the public key of the cert*/
    X509_set_pubkey(X509, &PKey);

    X509_NAME* Name = X509_get_subject_name(X509);

    /**Set cert metadata*/
    X509_NAME_add_entry_by_txt(Name, "C", MBSTRING_ASC, (unsigned char*)"GB", -1, -1, 0);
    X509_NAME_add_entry_by_txt(Name, "O", MBSTRING_ASC, (unsigned char*)"BeamMP Ltd.", -1, -1, 0);
    X509_NAME_add_entry_by_txt(Name, "CN", MBSTRING_ASC, (unsigned char*)"localhost", -1, -1, 0);

    X509_set_issuer_name(X509, Name);

    // TODO: Hashing with sha256 might cause problems, check later
    if (!X509_sign(X509, &PKey, EVP_sha1())) {
        X509_free(X509);
        beammp_error("Could not sign X.509 certificate.");
        throw std::runtime_error { std::string("X.509 certificate signing error") };
    }
    return X509;
}

void Http::Server::Tx509KeypairGenerator::GenerateAndWriteToDisk(const fs::path& KeyFilePath, const fs::path& CertFilePath) {
    // todo: generate directories for ssl keys
    FILE* KeyFile = std::fopen(reinterpret_cast<const char*>(KeyFilePath.c_str()), "wb");
    if (!KeyFile) {
        beammp_error("Could not create file 'key.pem', check your permissions");
        throw std::runtime_error("Could not create file 'key.pem'");
    }

    EVP_PKEY* PKey = Http::Server::Tx509KeypairGenerator::GenerateKey();

    bool WriteOpResult = PEM_write_PrivateKey(KeyFile, PKey, nullptr, nullptr, 0, nullptr, nullptr);
    fclose(KeyFile);

    if (!WriteOpResult) {
        beammp_error("Could not write to file 'key.pem', check your permissions");
        throw std::runtime_error("Could not write to file 'key.pem'");
    }

    FILE* CertFile = std::fopen(reinterpret_cast<const char*>(CertFilePath.c_str()), "wb"); // x509 file
    if (!CertFile) {
        beammp_error("Could not create file 'cert.pem', check your permissions");
        throw std::runtime_error("Could not create file 'cert.pem'");
    }

    X509* x509 = Http::Server::Tx509KeypairGenerator::GenerateCertificate(*PKey);
    WriteOpResult = PEM_write_X509(CertFile, x509);
    fclose(CertFile);

    if (!WriteOpResult) {
        beammp_error("Could not write to file 'cert.pem', check your permissions");
        throw std::runtime_error("Could not write to file 'cert.pem'");
    }
    EVP_PKEY_free(PKey);
    X509_free(x509);
    return;
}

bool Http::Server::Tx509KeypairGenerator::EnsureTLSConfigExists() {
    if (fs::is_regular_file(Application::Settings.SSLKeyPath)
        && fs::is_regular_file(Application::Settings.SSLCertPath)) {
        return true;
    } else {
        return false;
    }
}

void Http::Server::SetupEnvironment() {
    if (!Application::Settings.HTTPServerUseSSL) {
        return;
    }
    auto parent = fs::path(Application::Settings.SSLKeyPath).parent_path();
    if (!fs::exists(parent))
        fs::create_directories(parent);

    Application::TSettings defaultSettings {};
    if (!Tx509KeypairGenerator::EnsureTLSConfigExists()) {
        beammp_warn(std::string("No default TLS Key / Cert found. "
                                "IF YOU HAVE NOT MODIFIED THE SSLKeyPath OR SSLCertPath VALUES "
                                "THIS IS NORMAL ON FIRST STARTUP! BeamMP will generate it's own certs in the default directory "
                                "(Check for permissions or corrupted key-/certfile)"));
        Tx509KeypairGenerator::GenerateAndWriteToDisk(defaultSettings.SSLKeyPath, defaultSettings.SSLCertPath);
        Http::Server::THttpServerInstance::KeyFilePath = defaultSettings.SSLKeyPath;
        Http::Server::THttpServerInstance::CertFilePath = defaultSettings.SSLCertPath;
    } else {
        Http::Server::THttpServerInstance::KeyFilePath = Application::Settings.SSLKeyPath;
        Http::Server::THttpServerInstance::CertFilePath = Application::Settings.SSLCertPath;
    }
}

Http::Server::THttpServerInstance::THttpServerInstance() {
    Application::SetSubsystemStatus("HTTPServer", Application::Status::Starting);
    mThread = std::thread(&Http::Server::THttpServerInstance::operator(), this);
    mThread.detach();
}

void Http::Server::THttpServerInstance::operator()() {
    beammp_info("HTTP(S) Server started on port " + std::to_string(Application::Settings.HTTPServerPort));
    std::unique_ptr<httplib::Server> HttpLibServerInstance;
    if (Application::Settings.HTTPServerUseSSL) {
        HttpLibServerInstance = std::make_unique<httplib::SSLServer>(
            reinterpret_cast<const char*>(Http::Server::THttpServerInstance::CertFilePath.c_str()),
            reinterpret_cast<const char*>(Http::Server::THttpServerInstance::KeyFilePath.c_str()));
    } else {
        HttpLibServerInstance = std::make_unique<httplib::Server>();
    }
    // todo: make this IP agnostic so people can set their own IP
    /*HttpLibServerInstance->Get("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("<!DOCTYPE html><article><h1>Hello World!</h1><section><p>BeamMP Server can now serve HTTP requests!</p></section></article></html>", "text/html");
    });*/
    HttpLibServerInstance->Get("/health", [](const httplib::Request&, httplib::Response& res) {
        size_t SystemsGood = 0;
        size_t SystemsBad = 0;
        auto Statuses = Application::GetSubsystemStatuses();
        for (const auto& NameStatusPair : Statuses) {
            switch (NameStatusPair.second) {
            case Application::Status::Starting:
            case Application::Status::ShuttingDown:
            case Application::Status::Shutdown:
            case Application::Status::Good:
                SystemsGood++;
                break;
            case Application::Status::Bad:
                SystemsBad++;
                break;
            }
        }
        res.set_content(SystemsBad == 0 ? "0" : "1", "text/plain");
        res.status = 200;
    });

    /*
    HttpLibServerInstance->Get("/status", [](const httplib::Request&, httplib::Response& res) {
        try {
            json::Document response;
            response.SetObject();
            rapidjson::Document::AllocatorType& Allocator = response.GetAllocator();
            // add to response
            auto& Server = LuaAPI::MP::Engine->Server();
            size_t CarCount = 0;
            size_t GuestCount = 0;
            json::Value Array(rapidjson::kArrayType);
            LuaAPI::MP::Engine->Server().ForEachClient([&](std::weak_ptr<TClient> Client) -> bool {
                if (!Client.expired()) {
                    auto Locked = Client.lock();
                    CarCount += Locked->GetCarCount();
                    GuestCount += Locked->IsGuest() ? 1 : 0;
                    json::Value Player(json::kObjectType);
                    Player.AddMember("name", json::StringRef(Locked->GetName().c_str()), Allocator);
                    Player.AddMember("id", Locked->GetID(), Allocator);
                    Array.PushBack(Player, Allocator);
                }
                return true;
            });
            response.AddMember("players", Array, Allocator);
            response.AddMember("player_count", Server.ClientCount(), Allocator);
            response.AddMember("guest_count", GuestCount, Allocator);
            response.AddMember("car_count", CarCount, Allocator);

            // compile & send response
            json::StringBuffer sb;
            json::Writer<json::StringBuffer> writer(sb);
            response.Accept(writer);
            res.set_content(sb.GetString(), "application/json");
        } catch (const std::exception& e) {
            beammp_error("Exception in /status endpoint: " + std::string(e.what()));
            res.status = 500;
        }
    });
    */
    // magic endpoint
    HttpLibServerInstance->Get({ 0x2f, 0x6b, 0x69, 0x74, 0x74, 0x79 }, [](const httplib::Request&, httplib::Response& res) {
        res.set_content(std::string(Magic), "text/plain");
    });
    HttpLibServerInstance->set_mount_point("/", "./www");

    Application::SetSubsystemStatus("HTTPServer", Application::Status::Good);
    HttpLibServerInstance->listen("0.0.0.0", Application::Settings.HTTPServerPort);
}
