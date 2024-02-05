#pragma once

/// @file
/// This file holds the FileWatcher interface.

#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/date_time/posix_time/posix_time_duration.hpp>
#include <boost/system/detail/error_code.hpp>
#include <boost/thread/scoped_thread.hpp>
#include <boost/thread/synchronized_value.hpp>
#include <filesystem>
#include <functional>
#include <unordered_map>
#include <unordered_set>

/// The FileWatcher class watches a directory or a file for changes,
/// and then notifies the caller through a signal.
///
/// This is a pretty convoluted implementation, and you may find it difficult
/// to read. This is not intentional, but simplifying this would
/// cost more time than to write this excuse.
///
/// It operates as follows:
///
/// A boost::asio::deadline_timer is waited on asynchronously.
/// Once expired, this timer calls FileWatcher::on_tick.
/// That function then loops through all registered files and directories,
/// taking great care to follow symlinks, and tries to find a file which has changed.
/// It determines this by storing the last known modification time.
/// Once a file is found which has a new modification time, the FileWatcher::sig_file_changed
/// signal is fired, and all connected slots must take care to handle the signal.
class FileWatcher {
public:
    /// Constructs the FileWatcher to watch the given files every few seconds, as
    /// specified by the seconds argument.
    FileWatcher(unsigned seconds);
    /// Stops the thread via m_shutdown.
    ~FileWatcher();

    /// Add a file to watch. If this file changes, FileWatcher::sig_file_changed is triggered
    /// with the path to the file.
    void watch_file(const std::filesystem::path& path);
    /// Add a directory to watch. If any file in the directory, or any subdirectories, change,
    /// FileWatcher::sig_file_changed is triggered.
    void watch_files_in(const std::filesystem::path& dir);

private:
    /// Entry point for the timer thread.
    void thread_main();

    /// Called every time the timer runs out, watches for file changes, then starts
    /// a new timer.
    void on_tick(const boost::system::error_code&);

    /// Checks files for changes, calls FileWatcher::sig_file_changed on change.
    void check_files();
    /// Checks directories for files which changed, calls FileWatcher::sig_file_changed on change.
    void check_directories();
    /// Checks a single file for change.
    void check_file(const std::filesystem::path& file);

    /// Interval in seconds for the timer. Needed to be able to restart the timer over and over.
    boost::synchronized_value<boost::posix_time::seconds> m_seconds;
    /// If set, the thread handling the file watching will shut down. Set in the destructor.
    boost::synchronized_value<bool> m_shutdown { false };
    /// Io context handles the scheduling of timers on the thread.
    boost::asio::io_context m_io {};
    /// Holds all files that are to be checked.
    ///
    /// It uses a boost::hash<> because in the original C++17
    /// standard, std::hash of a filesystem path was not defined, and as such
    /// some implementations still don't have it.
    /// See https://cplusplus.github.io/LWG/issue3657
    boost::synchronized_value<std::unordered_set<std::filesystem::path, boost::hash<std::filesystem::path>>> m_files {};
    /// Holds all the directories that are to be searched for files to be checked.
    ///
    /// See FileWatcher::m_files for an explanation for the boost::hash.
    boost::synchronized_value<std::unordered_set<std::filesystem::path, boost::hash<std::filesystem::path>>> m_dirs {};
    /// Holds the last known modification times of all found files.
    std::unordered_map<std::filesystem::path, std::filesystem::file_time_type, boost::hash<std::filesystem::path>> m_file_mod_times {};
    /// Timer used to time the checks. Restarted every FileWatcher::m_seconds seconds.
    boost::synchronized_value<boost::asio::deadline_timer> m_timer;
    /// Work guard helps the io_context "sleep" while there is no work to be done - must be reset in the
    /// destructor in order to not cause work to be thrown away (though in this case we probably don't care).
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> m_work_guard = boost::asio::make_work_guard(m_io);
    /// Thread on which all watching and timing work runs.
    boost::scoped_thread<> m_thread;
};

