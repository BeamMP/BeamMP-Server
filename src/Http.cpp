#include "Http.h"

#include "Common.h"

#include <map>
#include <stdexcept>

// TODO: Add sentry error handling back

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

std::string Http::POST(const std::string& host, int port, const std::string& target, const std::string& body, const std::string& ContentType, unsigned int* status) {
    httplib::SSLClient client(host, port);
    client.enable_server_certificate_verification(false);
    client.set_address_family(AF_INET);
    auto res = client.Post(target.c_str(), body.c_str(), body.size(), ContentType.c_str());
    if (res) {
        if (status) {
            *status = res->status;
        }
        return res->body;
    } else {
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

std::string Http::Status::ToString(int code) {
    if (Map.find(code) != Map.end()) {
        return Map.at(code);
    } else {
        return std::to_string(code);
    }
}
// Http::Server::THttpServerInstance::THttpServerInstance() { }

EVP_PKEY* Http::Server::Tx509KeypairGenerator::generateKey() {
    /**
     * Allocate memory for the pkey
     */
    EVP_PKEY* pkey = EVP_PKEY_new();
    if (pkey == nullptr) {
        beammp_error("Could not allocate memory for X.509 private key (PKEY) generation.");
        throw std::runtime_error { std::string { "X.509 PKEY allocation error" } };
    }
    BIGNUM* nonce = BN_new();
    RSA* rsa = RSA_new();
    RSA_generate_key_ex(rsa, Crypto::RSA_DEFAULT_KEYLENGTH, nonce, nullptr);
    BN_free(nonce);
    if (!EVP_PKEY_assign_RSA(pkey, rsa)) {
        EVP_PKEY_free(pkey);
        beammp_error(std::string("Could not generate " + std::to_string(Crypto::RSA_DEFAULT_KEYLENGTH) + "-bit RSA key."));
        throw std::runtime_error { std::string("X.509 RSA key generation error") };
    }
    // todo: figure out if returning by reference instead of passing pointers is a security breach
    return pkey;
}
X509* Http::Server::Tx509KeypairGenerator::generateCertificate(EVP_PKEY& pkey) {
    X509* x509 = X509_new();
    if (x509 == nullptr) {
        X509_free(x509);
        beammp_error("Could not allocate memory for X.509 certificate generation.");
        throw std::runtime_error { std::string("X.509 certificate generation error") };
    }

    /**Set the metadata of the certificate*/
    ASN1_INTEGER_set(X509_get_serialNumber(x509), 1);

    /**Set the cert validity to a year*/
    X509_gmtime_adj(X509_get_notBefore(x509), 0);
    X509_gmtime_adj(X509_get_notAfter(x509), 31536000L);

    /**Set the public key of the cert*/
    X509_set_pubkey(x509, &pkey);

    X509_NAME* name = X509_get_subject_name(x509);

    /**Set cert metadata*/
    X509_NAME_add_entry_by_txt(name, "C", MBSTRING_ASC, (unsigned char*)"GB", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC, (unsigned char*)"BeamMP Ltd.", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (unsigned char*)"localhost", -1, -1, 0);

    X509_set_issuer_name(x509, name);

    // TODO: Hashing with sha256 might cause problems, check later
    if (!X509_sign(x509, &pkey, EVP_sha256())) {
        X509_free(x509);
        beammp_error("Could not sign X.509 certificate.");
        throw std::runtime_error { std::string("X.509 certificate signing error") };
    }
    return x509;
}
void Http::Server::Tx509KeypairGenerator::generateAndWriteToDisk() {
    FILE* KeyFile = std::fopen("key.pem", "wb");
    if (!KeyFile) {
        beammp_error("Could not create file 'key.pem', check your permissions");
        throw std::runtime_error("Could not create file 'key.pem'");
    }
    FILE* CertFile = std::fopen("cert.pem", "wb"); //x509 file
    if (!CertFile) {
        beammp_error("Could not create file 'cert.pem', check your permissions");
        throw std::runtime_error("Could not create file 'cert.pem'");
    }
    EVP_PKEY* pkey = Http::Server::Tx509KeypairGenerator::generateKey();

    bool writeOpResult = PEM_write_PrivateKey(KeyFile, pkey, nullptr, nullptr, 0, nullptr, nullptr);
    fclose(KeyFile);

    if(!writeOpResult){
        beammp_error("Could not write to file 'key.pem', check your permissions");
        throw std::runtime_error("Could not write to file 'key.pem'");
    }

    X509* x509 = Http::Server::Tx509KeypairGenerator::generateCertificate(*pkey);
    writeOpResult = PEM_write_X509(CertFile, x509);
    fclose(CertFile);

    if(!writeOpResult){
        beammp_error("Could not write to file 'cert.pem', check your permissions");
        throw std::runtime_error("Could not write to file 'cert.pem'");
    }
    EVP_PKEY_free(pkey);
    X509_free(x509);
    return;
}
