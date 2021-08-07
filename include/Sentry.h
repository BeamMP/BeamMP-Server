#ifndef SENTRY_H
#define SENTRY_H

#include <sentry.h>
#include <string>

enum class Logger {

};

// singleton, dont make this twice
class TSentry final {
public:
    TSentry(const std::string& SentryUrl);
    ~TSentry();

    void Log(sentry_level_t level, const std::string& logger, const std::string& text);
    void LogException(const std::exception& e, const std::string& file, const std::string& line);
    void AddErrorBreadcrumb(const std::string& msg, const std::string& file, const std::string& line);

private:
    bool mValid { true };
};

#endif // SENTRY_H
