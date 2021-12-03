#pragma once

#include <string>
#include <unordered_map>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <httplib.h>
#include <Common.h>

namespace Crypto{
constexpr size_t RSA_DEFAULT_KEYLENGTH{2048};
}

namespace Http {
std::string GET(const std::string& host, int port, const std::string& target, unsigned int* status = nullptr);
std::string POST(const std::string& host, int port, const std::string& target, const std::string& body, const std::string& ContentType, unsigned int* status = nullptr);
namespace Status {
    std::string ToString(int code);
}
const std::string ErrorString = "-1";

namespace Server{
    /*class THttpServerInstance{
    public:
        THttpServerInstance();

    private:
        //todo: this should initialize inline
        httplib::SSLServer mhttplibServerInstance;

    };*/
    //todo: likely unsafe, replace with something that's managed by a domain specific crypto library
    class Tx509KeypairGenerator{
    public:
        static X509* generateCertificate(EVP_PKEY& pkey);
        static EVP_PKEY* generateKey();
        static void generateAndWriteToDisk();
    };
}
}
