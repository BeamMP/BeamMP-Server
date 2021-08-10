#ifndef SENTRY_H
#define SENTRY_H

#include <sentry.h>

#include <string>
#include <mutex>

// TODO possibly use attach_stacktrace

// singleton, dont make this twice
class TSentry final {
public:
    TSentry(const std::string& SentryUrl);
    ~TSentry();

    void PrintWelcome();
    void SetupUser();
    void Log(sentry_level_t level, const std::string& logger, const std::string& text);
    void LogDebug(const std::string& text, const std::string& file, const std::string& line);
    void AddExtra(const std::string& key, const sentry_value_t& value);
    void AddExtra(const std::string& key, const std::string& value);
    void LogException(const std::exception& e, const std::string& file, const std::string& line);
    void AddErrorBreadcrumb(const std::string& msg, const std::string& file, const std::string& line);
    // cleared when Logged
    void SetTransaction(const std::string& id);

private:
    bool mValid { true };
    std::mutex mMutex;
};

#endif // SENTRY_H
