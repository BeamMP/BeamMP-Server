#include "Update.h"
#include "Common.h"
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <httplib.h>
#include <nlohmann/json.hpp>

#if defined(__linux)
#include <unistd.h>
#elif defined(WIN32)
#include <windows.h>
#endif

static bool ProgressReport(uint64_t current, uint64_t total) {
    if (current == total) {
        fmt::print("100% ({:.2f} / {:.2f} KiB)\n", float(current) / 1024.0f, float(total) / 1024.0f);
    }
    if (total != 0) {
        auto percent = (float(current) / float(total)) * 100.0f;
        fmt::print("{:3.0f}% ({:.2f} / {:.2f} KiB)\r", percent, float(current) / 1024.0f, float(total) / 1024.0f);
    }
    return true;
}

static std::unordered_map<std::string, std::string> sDistroMap = {
    { "debian:11", "-debian" },
    { "ubuntu:22.04", "-ubuntu" },
};

void Update::PerformUpdate(const std::string& InvokedAs) {
    using json = nlohmann::json;
    namespace http = httplib;

    constexpr auto GH = "api.github.com";
    std::string ReleasesAPI = "/repos/BeamMP/BeamMP-Server/releases";

    http::Headers APIHeaders {};
    APIHeaders.emplace("X-GitHub-Api-Version", "2022-11-28");
    APIHeaders.emplace("Accept", "application/vnd.github+json");

    http::SSLClient c(GH);
    c.set_read_timeout(std::chrono::seconds(30));

    beammp_infof("Checking for latest release...");
    // check if there is a new release
    auto Res = c.Get(ReleasesAPI + "/latest", APIHeaders);
    if (!Res || Res->status < 200 || Res->status >= 300) {
        beammp_errorf("Failed to fetch latest release: {}", to_string(Res.error()));
        std::exit(1);
    }

    Version NewVersion { 0, 0, 0 };

    json ReleaseInfo;

    try {
        ReleaseInfo = json::parse(Res->body);

        std::string TagName = ReleaseInfo["tag_name"].get<std::string>();
        if (!TagName.starts_with("v") || std::count(TagName.begin(), TagName.end(), '.') != 2) {
            beammp_errorf("Invalid version provided by GitHub: '{}', exiting", TagName);
            std::exit(1);
        }
        TagName = TagName.substr(1);
        auto Tag = Application::VersionStrToInts(TagName);
        NewVersion = Version { Tag[0], Tag[1], Tag[2] };
        beammp_infof("Latest release is v{}", NewVersion.AsString());
    } catch (const std::exception& e) {
        beammp_errorf("Failed to fetch latest release info from GitHub");
        beammp_errorf("Error: {}", e.what());
        std::exit(1);
    }

    // check if the new release is patch, minor or major
    auto Current = Application::ServerVersion();

    bool MajorOutdated = NewVersion.major > Current.major;
    bool MinorOutdated = !MajorOutdated && Application::IsOutdated(Current, NewVersion);

    if (!MajorOutdated && !MinorOutdated) {
        beammp_infof("BeamMP-Server is already the latest version (v{})!", Current.AsString());
        std::exit(0);
    } else {
        beammp_infof("New update available, updating from v{} to v{}", Current.AsString(), NewVersion.AsString());
    }

    // see https://github.com/cpredef/predef for information
#if !defined(__amd64__)     \
    && !defined(__amd64)    \
    && !defined(__x86_64__) \
    && !defined(__x86_64)   \
    && !defined(_M_X64)     \
    && !defined(_M_AMD64)
    beammp_errorf("BeamMP doesn't provide binaries for your CPU architecture (only x86_64). Please update manually");
#endif

    std::string Postfix = {};
// figure out current platform
#if WIN32
    beammp_infof("Current platform is Windows");
    Postfix = ".exe";
#elif __linux
    beammp_infof("Current platform is Linux, checking distribution");
    const std::string OsReleasePath = "/etc/os-release";

    std::string DistroID = "";
    std::string DistroVersion = "";

    if (fs::exists(OsReleasePath)) {
        std::ifstream OsRelease(OsReleasePath);
        std::string Line {};
        while (std::getline(OsRelease, Line)) {
            if (Line.starts_with("ID=")) {
                DistroID = Line.substr(3);
            } else if (Line.starts_with("VERSION_ID=\"")) {
                DistroVersion = Line.substr(strlen("VERSION_ID=\""));
                // skip closing quote
                DistroVersion = DistroVersion.substr(0, DistroVersion.size() - 1);
            } else if (Line.starts_with("VERSION_ID=")) {
                DistroVersion = Line.substr(strlen("VERSION_ID="));
            }
        }
    }
    beammp_infof("Distribution: {} {}", DistroID, DistroVersion);
    const auto Distro = DistroID + ":" + DistroVersion;

    if (sDistroMap.contains(Distro)) {
        Postfix = sDistroMap[Distro];
    } else {
        beammp_errorf("BeamMP doesn't provide binaries for this distribution, please update manually");
        std::exit(1);
    }
#else
    beammp_infof("BeamMP doesn't provide binaries for this platform, please update manually");
    std::exit(1);
#endif

    // check if the release exists for that platform
    std::string DownloadURL = "";
    try {
        for (const auto& Asset : ReleaseInfo.at("assets")) {
            if (Asset.at("name").get<std::string>() == "BeamMP-Server" + Postfix) {
                DownloadURL = Asset.at("browser_download_url");
                break;
            }
        }
    } catch (const std::exception& e) {
        beammp_errorf("Failed to parse GitHub API's release assets: {}", e.what());
        std::exit(1);
    }
    if (DownloadURL.empty()) {
        beammp_infof("BeamMP doesn't provide binaries for this platform or distribution (postfix '{}' not found in the release assets), please update manually", Postfix);
        std::exit(1);
    }

    // download urls exist, ask if the user wants to do a major update
    if (MajorOutdated) {
        beammp_warnf("The update from v{} to v{} is a major update, which is likely to *break* any Lua Plugins. Please make sure you have read the release notes at {} before proceeding!", Current.AsString(), NewVersion.AsString(), ReleaseInfo.at("html_url").get<std::string>());
#if !defined(WIN32)
        if (!isatty(STDIN_FILENO)) {
            beammp_errorf("Refusing to do a major version update non-interactively. Run this again in a TTY or update manually");
            std::exit(1);
        }
#endif
        fmt::print("\n");
        int ch;
        do {
            fmt::print("Do you wish to proceed with this update? [y/n] ");
            std::string Input;
            std::getline(std::cin, Input);
            if (Input.empty()) {
                continue;
            }
            ch = Input.at(0);
        } while (tolower(ch) != 'y' && tolower(ch) != 'n');
        if (tolower(ch) == 'n') {
            beammp_error("Cancelling update");
            std::exit(2);
        }
    }

    beammp_info("Downloading latest release from github.com...");

    http::SSLClient DlClient("github.com");
    DlClient.set_follow_location(true);
    auto ReleaseRes = DlClient.Get(DownloadURL.substr(strlen("https://github.com")), ProgressReport);

    if (!ReleaseRes || ReleaseRes->status < 200 || ReleaseRes->status >= 300) {
        beammp_errorf("Failed to fetch binary: {}. Please update manually", to_string(ReleaseRes.error()));
        std::exit(1);
    }

    beammp_info("Download complete!");

    auto Temp = InvokedAs + ".temp";
    beammp_infof("Creating '{}'", Temp);
    FILE* Out = std::fopen(Temp.c_str(), "w+");
    if (!Out) {
        beammp_errorf("Failed to update executable, because a temporary file couldn't be created: {}", std::strerror(errno));
        std::exit(1);
    }
    auto n = std::fwrite(ReleaseRes->body.data(), 1, ReleaseRes->body.size(), Out);
    if (n != ReleaseRes->body.size()) {
        beammp_errorf("Failed to update executable, because a temporary file couldn't be written to: {}", std::strerror(errno));
        std::fclose(Out);
        std::exit(1);
    }
    std::fclose(Out);

#if defined(__linux)
    beammp_infof("Removing '{}'", InvokedAs);
    struct stat st;
    if (stat(InvokedAs.c_str(), &st) != 0) {
        // shouldn't happen at this point
        beammp_errorf("Failed to stat original executable: {}", std::strerror(errno));
        std::exit(1);
    }
    auto Ret = unlink(InvokedAs.c_str());
    if (Ret != 0) {
        beammp_errorf("Failed to remove executable: {}", std::strerror(errno));
        std::exit(1);
    }
    beammp_infof("Replacing '{}' with '{}'", InvokedAs, Temp);
    fs::rename(Temp, InvokedAs);
    if (chmod(InvokedAs.c_str(), st.st_mode) != 0) {
        beammp_warnf("Failed to set file mode to 0{:o}: {}. File may not be executable.", st.st_mode, std::strerror(errno));
    }
#elif defined(WIN32)
    auto DeleteMe = InvokedAs + ".delete_me";
    std::filesystem::rename(InvokedAs, DeleteMe);
    std::filesystem::rename(Temp, InvokedAs);
    int Attr = GetFileAttributesA(DeleteMe.c_str());
    if ((Attr & FILE_ATTRIBUTE_HIDDEN) == 0) {
        SetFileAttributesA(DeleteMe.c_str(), Attr | FILE_ATTRIBUTE_HIDDEN);
    }
#else
    beammp_error("Not implemented");
    std::exit(4);
#endif

    // make sure the user knows that it was a success, on windows wait for return???

    std::exit(0);
}
