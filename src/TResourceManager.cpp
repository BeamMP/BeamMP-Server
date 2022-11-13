#include "TResourceManager.h"

#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

std::string TResourceManager::FormatForBackend(const ModMap& mods) {
    std::string monkey;
    for (const auto& [name, size] : mods) {
        monkey += fs::path(name).filename().string() + ';';
    }
    return monkey;
}

std::string TResourceManager::FormatForClient(const ModMap& mods) {
    std::string monkey;
    for (const auto& [name, size] : mods) {
        monkey += '/' + name + ';';
    }
    for (const auto& [name, size] : mods) {
        monkey += std::to_string(size) + ';';
    }
    return monkey;
}

/// @brief Sanitizes a requested mod string
/// @param pathString Raw mod path string
/// @param mods List of allowed mods for this client
/// @return Error, if any
std::optional<std::string> TResourceManager::IsModValid(std::string& pathString, const ModMap& mods) {
    auto path = fs::path(pathString);
    if (!path.has_filename()) {
        beammp_warn("File " + pathString + " is not a file!");
        return { "the requested file doesn't contain a valid filename" };
    }

    auto BasePath = fs::path(Application::GetSettingString(StrResourceFolder) + "/Client");

    auto CombinedPath = fs::path(BasePath.string() + pathString).lexically_normal();

    // beammp_infof("path: {}, base: {}, combined: {}", pathString, BasePath.string(), CombinedPath.string());

    if (!std::filesystem::exists(CombinedPath)) {
        beammp_warn("File " + pathString + " could not be accessed!");
        return { "the requested file doesn't exist or couldn't be accessed" };
    }

    auto relative = fs::relative(CombinedPath, BasePath);

    if (mods.count(relative.string()) == 0) {
        beammp_warn("File " + pathString + " is disallowed for this player!");
        return { "the requested file is disallowed for this player" };
    }

    pathString = CombinedPath.string();
    return {};
}

TResourceManager::TResourceManager() {
    Application::SetSubsystemStatus("ResourceManager", Application::Status::Starting);
    std::string BasePath = Application::GetSettingString(StrResourceFolder) + "/Client";
    if (!fs::exists(BasePath))
        fs::create_directories(BasePath);
    std::vector<std::string> modNames;

    auto iterator = fs::recursive_directory_iterator(BasePath, fs::directory_options::follow_directory_symlink | fs::directory_options::skip_permission_denied);
    for (const auto& entry : iterator) {
        if (iterator.depth() > 0 && !Application::GetSettingBool(StrIncludeSubdirectories))
            continue;
        if ((entry.is_regular_file() || entry.is_symlink()) && entry.path().extension() == ".zip") {
            auto relativePath = fs::relative(entry.path(), BasePath);
            beammp_infof("mod entry: {}", relativePath.string());

            mMods[relativePath.string()] = entry.file_size();
            mTotalModSize += entry.file_size();
        }
    }

    if (!mMods.empty()) {
        beammp_infof("Loaded {} mod{}", mMods.size(), mMods.size() != 1 ? 's' : ' ');
    }

    Application::SetSubsystemStatus("ResourceManager", Application::Status::Good);
}
