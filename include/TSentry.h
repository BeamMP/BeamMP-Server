#ifndef SENTRY_H
#define SENTRY_H

#include <mutex>
#include <string>
#include <unordered_map>
enum class SentryLevel {
    Debug = -1,
    Info = 0,
    Warning = 1,
    Error = 2,
    Fatal = 3,
};

// singleton, dont make this twice
class TSentry final {
public:
    TSentry();
    ~TSentry();

    void PrintWelcome();
    void SetupUser();
    void Log(SentryLevel level, const std::string& logger, const std::string& text);
    void LogError(const std::string& text, const std::string& file, const std::string& line);
    void SetContext(const std::string& context_name, const std::unordered_map<std::string, std::string>& map);
    void LogException(const std::exception& e, const std::string& file, const std::string& line);
    void LogAssert(const std::string& condition_string, const std::string& file, const std::string& line, const std::string& function);
    void AddErrorBreadcrumb(const std::string& msg, const std::string& file, const std::string& line);
    // cleared when Logged
    void SetTransaction(const std::string& id);
    [[nodiscard]] std::unique_lock<std::mutex> CreateExclusiveContext();

private:
    bool mValid { true };
    std::mutex mMutex;
};

#endif // SENTRY_H
