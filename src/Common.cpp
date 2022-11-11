#include "Common.h"

#include "TConsole.h"
#include <array>
#include <charconv>
#include <iostream>
#include <map>
#include <regex>
#include <sstream>
#include <thread>

#include "Compat.h"
#include "CustomAssert.h"
#include "Http.h"

Application::SettingsMap Application::mSettings = {
    { StrName, std::string("BeamMP Server") },
    { StrDescription, std::string("No description") },
    { StrResourceFolder, std::string("Resources") },
    { StrMap, std::string("/levels/gridmap_v2/info.json") },
    { StrSSLKeyPath, std::string("./.ssl/HttpServer/key.pem") },
    { StrSSLCertPath, std::string("./.ssl/HttpServer/cert.pem") },
    { StrHTTPServerEnabled, false },
    { StrMaxPlayers, int(8) },
    { StrPrivate, true },
    { StrMaxCars, int(1) },
    { StrDebug, false },
    { StrPort, int(30814) },
    { StrCustomIP, std::string("") },
    { StrLogChat, true },
    { StrSendErrors, true },
    { StrSendErrorsMessageEnabled, true },
    { StrHTTPServerPort, int(8080) },
    { StrHTTPServerIP, std::string("127.0.0.1") },
    { StrHTTPServerUseSSL, false },
    { StrHideUpdateMessages, false },
    { StrAuthKey, std::string("") },
};

// global, yes, this is ugly, no, it cant be done another way
TSentry Sentry {};

std::string Application::SettingToString(const Application::SettingValue& Value) {
    switch (Value.which()) {
    case 0:
        return fmt::format("{}", boost::get<std::string>(Value));
    case 1:
        return fmt::format("{}", boost::get<bool>(Value));
    case 2:
        return fmt::format("{}", boost::get<int>(Value));
    default:
        return "<unknown type>";
    }
}

std::string Application::GetSettingString(std::string_view Name) {
    try {
        return boost::get<std::string>(Application::mSettings.at(Name));
    } catch (const std::exception& e) {
        beammp_errorf("Failed to get string setting '{}': {}", Name, e.what());
        return "";
    }
}

int Application::GetSettingInt(std::string_view Name) {
    try {
        return boost::get<int>(Application::mSettings.at(Name));
    } catch (const std::exception& e) {
        beammp_errorf("Failed to get int setting '{}': {}", Name, e.what());
        return 0;
    }
}

bool Application::GetSettingBool(std::string_view Name) {
    try {
        return boost::get<bool>(Application::mSettings.at(Name));
    } catch (const std::exception& e) {
        beammp_errorf("Failed to get bool setting '{}': {}", Name, e.what());
        return false;
    }
}

void Application::SetSetting(std::string_view Name, const Application::SettingValue& Value) {
    if (mSettings.contains(Name)) {
        if (mSettings[Name].type() == Value.type()) {
            mSettings[Name] = Value;
        } else {
            beammp_errorf("Could not change value of setting '{}', because it has a different type.", Name);
        }
    } else {
        mSettings[Name] = Value;
    }
}

void Application::RegisterShutdownHandler(const TShutdownHandler& Handler) {
    std::unique_lock Lock(mShutdownHandlersMutex);
    if (Handler) {
        mShutdownHandlers.push_front(Handler);
    }
}

void Application::GracefullyShutdown() {
    SetShutdown(true);
    static bool AlreadyShuttingDown = false;
    static uint8_t ShutdownAttempts = 0;
    if (AlreadyShuttingDown) {
        ++ShutdownAttempts;
        // hard shutdown at 2 additional tries
        if (ShutdownAttempts == 2) {
            beammp_info("hard shutdown forced by multiple shutdown requests");
            std::exit(0);
        }
        beammp_info("already shutting down!");
        return;
    } else {
        AlreadyShuttingDown = true;
    }
    beammp_trace("waiting for lock release");
    std::unique_lock Lock(mShutdownHandlersMutex);
    beammp_info("please wait while all subsystems are shutting down...");
    for (size_t i = 0; i < mShutdownHandlers.size(); ++i) {
        beammp_info("Subsystem " + std::to_string(i + 1) + "/" + std::to_string(mShutdownHandlers.size()) + " shutting down");
        mShutdownHandlers[i]();
    }
    // std::exit(-1);
}

std::string Application::ServerVersionString() {
    return mVersion.AsString();
}

std::array<uint8_t, 3> Application::VersionStrToInts(const std::string& str) {
    std::array<uint8_t, 3> Version;
    std::stringstream ss(str);
    for (uint8_t& i : Version) {
        std::string Part;
        std::getline(ss, Part, '.');
        std::from_chars(&*Part.begin(), &*Part.begin() + Part.size(), i);
    }
    return Version;
}

TEST_CASE("Application::VersionStrToInts") {
    auto v = Application::VersionStrToInts("1.2.3");
    CHECK(v[0] == 1);
    CHECK(v[1] == 2);
    CHECK(v[2] == 3);

    v = Application::VersionStrToInts("10.20.30");
    CHECK(v[0] == 10);
    CHECK(v[1] == 20);
    CHECK(v[2] == 30);

    v = Application::VersionStrToInts("100.200.255");
    CHECK(v[0] == 100);
    CHECK(v[1] == 200);
    CHECK(v[2] == 255);
}

bool Application::IsOutdated(const Version& Current, const Version& Newest) {
    if (Newest.major > Current.major) {
        return true;
    } else if (Newest.major == Current.major && Newest.minor > Current.minor) {
        return true;
    } else if (Newest.major == Current.major && Newest.minor == Current.minor && Newest.patch > Current.patch) {
        return true;
    } else {
        return false;
    }
}

bool Application::IsShuttingDown() {
    std::shared_lock Lock(mShutdownMtx);
    return mShutdown;
}

void Application::SleepSafeSeconds(size_t Seconds) {
    // Sleeps for 500 ms, checks if a shutdown occurred, and so forth
    for (size_t i = 0; i < Seconds * 2; ++i) {
        if (Application::IsShuttingDown()) {
            return;
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }
}

TEST_CASE("Application::IsOutdated (version check)") {
    SUBCASE("Same version") {
        CHECK(!Application::IsOutdated({ 1, 2, 3 }, { 1, 2, 3 }));
    }
    // we need to use over 1-2 digits to test against lexical comparisons
    SUBCASE("Patch outdated") {
        for (uint8_t Patch = 0; Patch < 10; ++Patch) {
            for (uint8_t Minor = 0; Minor < 10; ++Minor) {
                for (uint8_t Major = 0; Major < 10; ++Major) {
                    CHECK(Application::IsOutdated({ uint8_t(Major), uint8_t(Minor), uint8_t(Patch) }, { uint8_t(Major), uint8_t(Minor), uint8_t(Patch + 1) }));
                }
            }
        }
    }
    SUBCASE("Minor outdated") {
        for (uint8_t Patch = 0; Patch < 10; ++Patch) {
            for (uint8_t Minor = 0; Minor < 10; ++Minor) {
                for (uint8_t Major = 0; Major < 10; ++Major) {
                    CHECK(Application::IsOutdated({ uint8_t(Major), uint8_t(Minor), uint8_t(Patch) }, { uint8_t(Major), uint8_t(Minor + 1), uint8_t(Patch) }));
                }
            }
        }
    }
    SUBCASE("Major outdated") {
        for (uint8_t Patch = 0; Patch < 10; ++Patch) {
            for (uint8_t Minor = 0; Minor < 10; ++Minor) {
                for (uint8_t Major = 0; Major < 10; ++Major) {
                    CHECK(Application::IsOutdated({ uint8_t(Major), uint8_t(Minor), uint8_t(Patch) }, { uint8_t(Major + 1), uint8_t(Minor), uint8_t(Patch) }));
                }
            }
        }
    }
    SUBCASE("All outdated") {
        for (uint8_t Patch = 0; Patch < 10; ++Patch) {
            for (uint8_t Minor = 0; Minor < 10; ++Minor) {
                for (uint8_t Major = 0; Major < 10; ++Major) {
                    CHECK(Application::IsOutdated({ uint8_t(Major), uint8_t(Minor), uint8_t(Patch) }, { uint8_t(Major + 1), uint8_t(Minor + 1), uint8_t(Patch + 1) }));
                }
            }
        }
    }
}

void Application::SetSubsystemStatus(const std::string& Subsystem, Status status) {
    switch (status) {
    case Status::Good:
        beammp_trace("Subsystem '" + Subsystem + "': Good");
        break;
    case Status::Bad:
        beammp_trace("Subsystem '" + Subsystem + "': Bad");
        break;
    case Status::Starting:
        beammp_trace("Subsystem '" + Subsystem + "': Starting");
        break;
    case Status::ShuttingDown:
        beammp_trace("Subsystem '" + Subsystem + "': Shutting down");
        break;
    case Status::Shutdown:
        beammp_trace("Subsystem '" + Subsystem + "': Shutdown");
        break;
    default:
        beammp_assert_not_reachable();
    }
    std::unique_lock Lock(mSystemStatusMapMutex);
    mSystemStatusMap[Subsystem] = status;
}

void Application::SetShutdown(bool Val) {
    std::unique_lock Lock(mShutdownMtx);
    mShutdown = Val;
}

TEST_CASE("Application::SetSubsystemStatus") {
    Application::SetSubsystemStatus("Test", Application::Status::Good);
    auto Map = Application::GetSubsystemStatuses();
    CHECK(Map.at("Test") == Application::Status::Good);
    Application::SetSubsystemStatus("Test", Application::Status::Bad);
    Map = Application::GetSubsystemStatuses();
    CHECK(Map.at("Test") == Application::Status::Bad);
}

void Application::CheckForUpdates() {
    Application::SetSubsystemStatus("UpdateCheck", Application::Status::Starting);
    static bool FirstTime = true;
    // checks current version against latest version
    std::regex VersionRegex { R"(\d+\.\d+\.\d+\n*)" };
    for (const auto& url : GetBackendUrlsInOrder()) {
        auto Response = Http::GET(url, 443, "/v/s");
        bool Matches = std::regex_match(Response, VersionRegex);
        if (Matches) {
            auto MyVersion = ServerVersion();
            auto RemoteVersion = Version(VersionStrToInts(Response));
            if (IsOutdated(MyVersion, RemoteVersion)) {
                std::string RealVersionString = RemoteVersion.AsString();
                beammp_warn(std::string(ANSI_YELLOW_BOLD) + "NEW VERSION IS OUT! Please update to the new version (v" + RealVersionString + ") of the BeamMP-Server! Download it here: https://beammp.com/! For a guide on how to update, visit: https://wiki.beammp.com/en/home/server-maintenance#updating-the-server" + std::string(ANSI_RESET));
            } else {
                if (FirstTime) {
                    beammp_info("Server up-to-date!");
                }
            }
            Application::SetSubsystemStatus("UpdateCheck", Application::Status::Good);
            break;
        } else {
            if (FirstTime) {
                beammp_debug("Failed to fetch version from: " + url);
                beammp_trace("got " + Response);
                auto Lock = Sentry.CreateExclusiveContext();
                Sentry.SetContext("get-response", { { "response", Response } });
                Sentry.LogError("failed to get server version", _file_basename, _line);
                Application::SetSubsystemStatus("UpdateCheck", Application::Status::Bad);
            }
        }
    }
    if (Application::GetSubsystemStatuses().at("UpdateCheck") == Application::Status::Bad) {
        if (FirstTime) {
            beammp_warn("Unable to fetch version info from backend.");
        }
    }
    FirstTime = false;
}

// thread name stuff

static std::map<std::thread::id, std::string> threadNameMap {};
static std::mutex ThreadNameMapMutex {};

std::string ThreadName(bool DebugModeOverride) {
    auto Lock = std::unique_lock(ThreadNameMapMutex);
    if (DebugModeOverride || Application::GetSettingBool(StrDebug)) {
        auto id = std::this_thread::get_id();
        if (threadNameMap.find(id) != threadNameMap.end()) {
            // found
            return threadNameMap.at(id) + " ";
        }
    }
    return "";
}

TEST_CASE("ThreadName") {
    RegisterThread("MyThread");
    auto OrigDebug = Application::GetSettingBool(StrDebug);

    // ThreadName adds a space at the end, legacy but we need it still
    SUBCASE("Debug mode enabled") {
        Application::SetSetting(StrDebug, true);
        CHECK(ThreadName(true) == "MyThread ");
        CHECK(ThreadName(false) == "MyThread ");
    }
    SUBCASE("Debug mode disabled") {
        Application::SetSetting(StrDebug, false);
        CHECK(ThreadName(true) == "MyThread ");
        CHECK(ThreadName(false) == "");
    }
    // cleanup
    Application::SetSetting(StrDebug, OrigDebug);
}

void RegisterThread(const std::string& str) {
    std::string ThreadId;
#ifdef BEAMMP_WINDOWS
    ThreadId = std::to_string(GetCurrentThreadId());
#elif defined(BEAMMP_APPLE)
    ThreadId = std::to_string(getpid()); // todo: research if 'getpid()' is a valid, posix compliant alternative to 'gettid()'
#elif defined(BEAMMP_LINUX)
    ThreadId = std::to_string(gettid());
#endif
    if (Application::GetSettingBool(StrDebug)) {
        std::ofstream ThreadFile(".Threads.log", std::ios::app);
        ThreadFile << ("Thread \"" + str + "\" is TID " + ThreadId) << std::endl;
    }
    auto Lock = std::unique_lock(ThreadNameMapMutex);
    threadNameMap[std::this_thread::get_id()] = str;
}

TEST_CASE("RegisterThread") {
    RegisterThread("MyThread");
    CHECK(threadNameMap.at(std::this_thread::get_id()) == "MyThread");
}

Version::Version(uint8_t major, uint8_t minor, uint8_t patch)
    : major(major)
    , minor(minor)
    , patch(patch) { }

Version::Version(const std::array<uint8_t, 3>& v)
    : Version(v[0], v[1], v[2]) {
}

std::string Version::AsString() {
    return fmt::format("{:d}.{:d}.{:d}", major, minor, patch);
}

TEST_CASE("Version::AsString") {
    CHECK(Version { 0, 0, 0 }.AsString() == "0.0.0");
    CHECK(Version { 1, 2, 3 }.AsString() == "1.2.3");
    CHECK(Version { 255, 255, 255 }.AsString() == "255.255.255");
}

void LogChatMessage(const std::string& name, int id, const std::string& msg) {
    if (Application::GetSettingBool(StrLogChat)) {
        std::stringstream ss;
        ss << ThreadName();
        ss << "[CHAT] ";
        if (id != -1) {
            ss << "(" << id << ") <" << name << "> ";
        } else {
            ss << name << "";
        }
        ss << msg;
#ifdef DOCTEST_CONFIG_DISABLE
        Application::Console().Write(ss.str());
#endif
    }
}

std::string GetPlatformAgnosticErrorString() {
#ifdef BEAMMP_WINDOWS
    // This will provide us with the error code and an error message, all in one.
    int err;
    char msgbuf[256];
    msgbuf[0] = '\0';

    err = GetLastError();

    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        err,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        msgbuf,
        sizeof(msgbuf),
        nullptr);

    if (*msgbuf) {
        return std::to_string(GetLastError()) + " - " + std::string(msgbuf);
    } else {
        return std::to_string(GetLastError());
    }
#elif defined(BEAMMP_LINUX) || defined(BEAMMP_APPLE)
    return std::strerror(errno);
#else
    return "(no human-readable errors on this platform)";
#endif
}

std::string ToHumanReadableSize(size_t Size) {
    if (Size > TB) {
        return fmt::format("{:.2f} TiB", double(Size) / TB);
    } else if (Size > GB) {
        return fmt::format("{:.2f} GiB", double(Size) / GB);
    } else if (Size > MB) {
        return fmt::format("{:.2f} MiB", double(Size) / MB);
    } else if (Size > KB) {
        return fmt::format("{:.2f} KiB", double(Size) / KB);
    } else {
        return fmt::format("{} B", Size);
    }
}
