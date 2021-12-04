#pragma once

#include <Common.h>
#include <IThreaded.h>
#include <filesystem>
#include <httplib.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <string>
#include <unordered_map>

namespace fs = std::filesystem;

namespace Crypto {
constexpr size_t RSA_DEFAULT_KEYLENGTH { 2048 };
}

namespace Http {
std::string GET(const std::string& host, int port, const std::string& target, unsigned int* status = nullptr);
std::string POST(const std::string& host, int port, const std::string& target, const std::string& body, const std::string& ContentType, unsigned int* status = nullptr);
namespace Status {
    std::string ToString(int code);
}
const std::string ErrorString = "-1";

namespace Server {
    void SetupEnvironment();
    // todo: Add non TLS Server Instance, this one is TLS only
    class THttpServerInstance : IThreaded {
    public:
        THttpServerInstance();
        static fs::path KeyFilePath;
        static fs::path CertFilePath;

    protected:
        void operator()();

    private:
        /**
         * the shared pointer is necessary because httplib is a blocking library and due lacking thread-safety
         * will "forget" about its environment, when configured across multiple threads.
         * So we need to able to start the server (make it "listen()") in a single Thread.
         */
        std::shared_ptr<httplib::SSLServer> mHttpLibServerInstancePtr;
    };
    // todo: all of these functions are likely unsafe,
    // todo: replace with something that's managed by a domain specific crypto library
    class Tx509KeypairGenerator {
    public:
        static bool EnsureTLSConfigExists();
        static X509* GenerateCertificate(EVP_PKEY& pkey);
        static EVP_PKEY* GenerateKey();
        static void GenerateAndWriteToDisk(const fs::path& KeyFilePath, const fs::path& CertFilePath);
    };
}
}
