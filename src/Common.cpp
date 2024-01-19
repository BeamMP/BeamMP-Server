// BeamMP, the BeamNG.drive multiplayer mod.
// Copyright (C) 2024 BeamMP Ltd., BeamMP team and contributors.
//
// BeamMP Ltd. can be contacted by electronic mail via contact@beammp.com.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "Common.h"

#include "Env.h"
#include "TConsole.h"
#include <array>
#include <charconv>
#include <fmt/core.h>
#include <iostream>
#include <map>
#include <regex>
#include <sstream>
#include <thread>

#include "Compat.h"
#include "CustomAssert.h"
#include "Http.h"

Application::TSettings Application::Settings = {};

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
                std::string RealVersionString = std::string("v") + RemoteVersion.AsString();
                const std::string DefaultUpdateMsg = "NEW VERSION IS OUT! Please update to the new version ({}) of the BeamMP-Server! Download it here: https://beammp.com/! For a guide on how to update, visit: https://wiki.beammp.com/en/home/server-maintenance#updating-the-server";
                auto UpdateMsg = Env::Get(Env::Key::PROVIDER_UPDATE_MESSAGE).value_or(DefaultUpdateMsg);
                UpdateMsg = fmt::vformat(std::string_view(UpdateMsg), fmt::make_format_args(RealVersionString));
                beammp_warnf("{}{}{}", ANSI_YELLOW_BOLD, UpdateMsg, ANSI_RESET);
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
    if (DebugModeOverride || Application::Settings.DebugModeEnabled) {
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
    auto OrigDebug = Application::Settings.DebugModeEnabled;

    // ThreadName adds a space at the end, legacy but we need it still
    SUBCASE("Debug mode enabled") {
        Application::Settings.DebugModeEnabled = true;
        CHECK(ThreadName(true) == "MyThread ");
        CHECK(ThreadName(false) == "MyThread ");
    }
    SUBCASE("Debug mode disabled") {
        Application::Settings.DebugModeEnabled = false;
        CHECK(ThreadName(true) == "MyThread ");
        CHECK(ThreadName(false) == "");
    }
    // cleanup
    Application::Settings.DebugModeEnabled = OrigDebug;
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
    if (Application::Settings.DebugModeEnabled) {
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
    if (Application::Settings.LogChat) {
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

// TODO: add unit tests to SplitString
void SplitString(const std::string& str, const char delim, std::vector<std::string>& out) {
    size_t start;
    size_t end = 0;

    while ((start = str.find_first_not_of(delim, end)) != std::string::npos) {
        end = str.find(delim, start);
        out.push_back(str.substr(start, end - start));
    }
}
