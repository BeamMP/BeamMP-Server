#include "FileWatcher.h"
#include "Common.h"
#include <chrono>
#include <filesystem>

/// @file
/// This file holds the FileWatcher implementation.

FileWatcher::FileWatcher(unsigned int seconds)
    : m_seconds(boost::posix_time::seconds(seconds))
    , m_timer(boost::asio::deadline_timer(m_io, *m_seconds)) {
    m_timer->async_wait([this](const auto& err) { on_tick(err); });

    m_thread = boost::scoped_thread<> { &FileWatcher::thread_main, this };
}

FileWatcher::~FileWatcher() {
    m_work_guard.reset();
    *m_shutdown = true;
}

void FileWatcher::watch_file(const std::filesystem::path& path) {
    m_files->insert(path);
}

void FileWatcher::watch_files_in(const std::filesystem::path& path) {
    m_dirs->insert(path);
}

void FileWatcher::thread_main() {
    while (!*m_shutdown) {
        m_io.run_for(std::chrono::seconds(1));
    }
}

void FileWatcher::on_tick(const boost::system::error_code& err) {
    auto timer = m_timer.synchronize();
    // set up timer so that the operations after this don't impact the accuracy of the
    // timing
    timer->expires_at(timer->expires_at() + *m_seconds);

    if (err) {
        l::error("FileWatcher encountered error: {}", err.message());
        // TODO: Should any further action be taken?
    } else {
        try {
            check_files();
        } catch (const std::exception& e) {
            l::error("FileWatcher exception while checking files: {}", e.what());
        }
        try {
            check_directories();
        } catch (const std::exception& e) {
            l::error("FileWatcher exception while checking directories: {}", e.what());
        }
    }
    // finally start the timer again, deadline has already been set at the beginning
    // of this function
    timer->async_wait([this](const auto& err) { on_tick(err); });
}

void FileWatcher::check_files() {
    auto files = m_files.synchronize();
    for (const auto& file : *files) {
        check_file(file);
        // TODO: add deleted/created watches
    }
}

void FileWatcher::check_directories() {
    auto directories = m_dirs.synchronize();
    for (const auto& dir : *directories) {
        if (std::filesystem::exists(dir)) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(dir, std::filesystem::directory_options::follow_directory_symlink | std::filesystem::directory_options::skip_permission_denied)) {
                if (entry.is_regular_file() || entry.is_symlink()) {
                    check_file(entry.path());
                }
            }
        }
        // TODO: add deleted/created watches
    }
}

void FileWatcher::check_file(const std::filesystem::path& file) {
    if (std::filesystem::exists(file)) {
        auto real_file = file;
        if (std::filesystem::is_symlink(file)) {
            real_file = std::filesystem::read_symlink(file);
        }
        auto time = std::filesystem::last_write_time(real_file);
        if (!m_file_mod_times.contains(file)) {
            m_file_mod_times.insert_or_assign(file, time);
        } else {
            if (m_file_mod_times.at(file) != time) {
                beammp_tracef("File changed: {}", file);
                m_file_mod_times.at(file) = time;
            }
        }
    }
}

