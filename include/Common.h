#pragma once

#include "TSentry.h"
extern TSentry Sentry;

#include <array>
#include <atomic>
#include <cstring>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <sstream>
#include <zlib.h>

#include "Compat.h"

#include "TConsole.h"

struct Version {
    uint8_t major;
    uint8_t minor;
    uint8_t patch;
    Version(uint8_t major, uint8_t minor, uint8_t patch);
    Version(const std::array<uint8_t, 3>& v);
    std::string AsString();
};

// static class handling application start, shutdown, etc.
// yes, static classes, singletons, globals are all pretty
// bad idioms. In this case we need a central way to access
// stuff like graceful shutdown, global settings (its in the name),
// etc.
class Application final {
public:
    // types
    struct TSettings {
        TSettings() noexcept
            : ServerName("BeamMP Server")
            , ServerDesc("BeamMP Default Description")
            , Resource("Resources")
            , MapName("/levels/gridmap_v2/info.json")
            , SSLKeyPath("./.ssl/HttpServer/key.pem")
            , SSLCertPath("./.ssl/HttpServer/cert.pem")
            , HTTPServerPort(8080)
            , MaxPlayers(10)
            , Private(true)
            , MaxCars(1)
            , DebugModeEnabled(false)
            , Port(30814)
            , SendErrors(true)
            , SendErrorsMessageEnabled(true) { }
        std::string ServerName;
        std::string ServerDesc;
        std::string Resource;
        std::string MapName;
        std::string Key;
        std::string SSLKeyPath;
        std::string SSLCertPath;
        int MaxPlayers;
        bool Private;
        int MaxCars;
        bool DebugModeEnabled;
        int Port;
        std::string CustomIP;
        bool SendErrors;
        bool SendErrorsMessageEnabled;
        [[nodiscard]] bool HasCustomIP() const { return !CustomIP.empty(); }
        int HTTPServerPort;
    };

    using TShutdownHandler = std::function<void()>;

    // methods
    Application() = delete;

    // 'Handler' is called when GracefullyShutdown is called
    static void RegisterShutdownHandler(const TShutdownHandler& Handler);
    // Causes all threads to finish up and exit gracefull gracefully
    static void GracefullyShutdown();
    static TConsole& Console() { return *mConsole; }
    static std::string ServerVersionString();
    static const Version& ServerVersion() { return mVersion; }
    static std::string ClientVersionString() { return "2.0"; }
    static std::string PPS() { return mPPS; }
    static void SetPPS(const std::string& NewPPS) { mPPS = NewPPS; }

    static inline TSettings Settings {};

    static std::string GetBackendUrlForAuth() { return "auth.beammp.com"; }
    static std::string GetBackendHostname() { return "backend.beammp.com"; }
    static std::string GetBackup1Hostname() { return "backup1.beammp.com"; }
    static std::string GetBackup2Hostname() { return "backup2.beammp.com"; }
    static std::string GetBackendUrlForSocketIO() { return "https://backend.beammp.com"; }
    static void CheckForUpdates();
    static std::array<uint8_t, 3> VersionStrToInts(const std::string& str);
    static bool IsOutdated(const Version& Current, const Version& Newest);

private:
    static inline std::string mPPS;
    static std::unique_ptr<TConsole> mConsole;
    static inline std::mutex mShutdownHandlersMutex {};
    static inline std::deque<TShutdownHandler> mShutdownHandlers {};

    static inline Version mVersion { 2, 4, 0 };
};

std::string ThreadName(bool DebugModeOverride = false);
void RegisterThread(const std::string& str);
#define RegisterThreadAuto() RegisterThread(__func__)

#define KB 1024
#define MB (KB * 1024)
#define SSU_UNRAW SECRET_SENTRY_URL

#define _file_basename std::filesystem::path(__FILE__).filename().string()
#define _line std::to_string(__LINE__)
#define _in_lambda (std::string(__func__) == "operator()")

// we would like the full function signature 'void a::foo() const'
// on windows this is __FUNCSIG__, on GCC it's __PRETTY_FUNCTION__,
// feel free to add more
#if defined(WIN32)
#define _function_name std::string(__FUNCSIG__)
#elif defined(__unix) || defined(__unix__)
#define _function_name std::string(__PRETTY_FUNCTION__)
#else
#define _function_name std::string(__func__)
#endif

#if defined(DEBUG)

// if this is defined, we will show the full function signature infront of
// each info/debug/warn... call instead of the 'filename:line' format.
#if defined(BMP_FULL_FUNCTION_NAMES)
#define _this_location (ThreadName() + _function_name + " ")
#else
#define _this_location (ThreadName() + _file_basename + ":" + _line + " ")
#endif
#define SU_RAW SSU_UNRAW

#else // !defined(DEBUG)

#define SU_RAW RAWIFY(SSU_UNRAW)
#define _this_location (ThreadName())

#endif // defined(DEBUG)

#define beammp_warn(x) Application::Console().Write(_this_location + std::string("[WARN] ") + (x))
#define beammp_info(x) Application::Console().Write(_this_location + std::string("[INFO] ") + (x))
#define beammp_error(x)                                                               \
    do {                                                                              \
        Application::Console().Write(_this_location + std::string("[ERROR] ") + (x)); \
        Sentry.AddErrorBreadcrumb((x), _file_basename, _line);                        \
    } while (false)
#define beammp_lua_error(x)                                                               \
    do {                                                                                  \
        Application::Console().Write(_this_location + std::string("[LUA_ERROR] ") + (x)); \
    } while (false)
#define luaprint(x) Application::Console().Write(_this_location + std::string("[LUA] ") + (x))
#define beammp_debug(x)                                                                   \
    do {                                                                                  \
        if (Application::Settings.DebugModeEnabled) {                                     \
            Application::Console().Write(_this_location + std::string("[DEBUG] ") + (x)); \
        }                                                                                 \
    } while (false)
#define beammp_event(x)                                                                   \
    do {                                                                                  \
        if (Application::Settings.DebugModeEnabled) {                                     \
            Application::Console().Write(_this_location + std::string("[EVENT] ") + (x)); \
        }                                                                                 \
    }while(false)
// for those times when you just need to ignore something :^)
// explicity disables a [[nodiscard]] warning
#define beammp_ignore(x) (void)x
// trace() is a debug-build debug()
#if defined(DEBUG)
#define beammp_trace(x)                                                                   \
    do {                                                                                  \
        if (Application::Settings.DebugModeEnabled) {                                     \
            Application::Console().Write(_this_location + std::string("[TRACE] ") + (x)); \
        }                                                                                 \
    } while (false)
#else
#define beammp_trace(x)
#endif // defined(DEBUG)

void LogChatMessage(const std::string& name, int id, const std::string& msg);

#define Biggest 30000

template <typename T>
inline T Comp(const T& Data) {
    std::array<char, Biggest> C {};
    // obsolete
    C.fill(0);
    z_stream defstream;
    defstream.zalloc = Z_NULL;
    defstream.zfree = Z_NULL;
    defstream.opaque = Z_NULL;
    defstream.avail_in = (uInt)Data.size();
    defstream.next_in = (Bytef*)&Data[0];
    defstream.avail_out = Biggest;
    defstream.next_out = reinterpret_cast<Bytef*>(C.data());
    deflateInit(&defstream, Z_BEST_COMPRESSION);
    deflate(&defstream, Z_SYNC_FLUSH);
    deflate(&defstream, Z_FINISH);
    deflateEnd(&defstream);
    size_t TotalOut = defstream.total_out;
    T Ret;
    Ret.resize(TotalOut);
    std::fill(Ret.begin(), Ret.end(), 0);
    std::copy_n(C.begin(), TotalOut, Ret.begin());
    return Ret;
}

template <typename T>
inline T DeComp(const T& Compressed) {
    std::array<char, Biggest> C {};
    // not needed
    C.fill(0);
    z_stream infstream;
    infstream.zalloc = Z_NULL;
    infstream.zfree = Z_NULL;
    infstream.opaque = Z_NULL;
    infstream.avail_in = Biggest;
    infstream.next_in = (Bytef*)(&Compressed[0]);
    infstream.avail_out = Biggest;
    infstream.next_out = (Bytef*)(C.data());
    inflateInit(&infstream);
    inflate(&infstream, Z_SYNC_FLUSH);
    inflate(&infstream, Z_FINISH);
    inflateEnd(&infstream);
    size_t TotalOut = infstream.total_out;
    T Ret;
    Ret.resize(TotalOut);
    std::fill(Ret.begin(), Ret.end(), 0);
    std::copy_n(C.begin(), TotalOut, Ret.begin());
    return Ret;
}

std::string GetPlatformAgnosticErrorString();
#define S_DSN SU_RAW

void LogChatMessage(const std::string& name, int id, const std::string& msg);
