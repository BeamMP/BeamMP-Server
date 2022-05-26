#pragma once

#include <Common.h>
#include <IThreaded.h>
#include <filesystem>
#include <string>
#include <unordered_map>

#if defined(BEAMMP_LINUX)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
#include <httplib.h>
#if defined(BEAMMP_LINUX)
#pragma GCC diagnostic pop
#endif

namespace fs = std::filesystem;

namespace Http {
std::string GET(const std::string& host, int port, const std::string& target, unsigned int* status = nullptr);
std::string POST(const std::string& host, int port, const std::string& target, const std::string& body, const std::string& ContentType, unsigned int* status = nullptr, const httplib::Headers& headers = {});
namespace Status {
    std::string ToString(int code);
}
const std::string ErrorString = "-1";

namespace Server {
    class THttpServerInstance {
    public:
        THttpServerInstance();

    protected:
        void operator()();

    private:
        std::thread mThread;
    };
}
}
