#include "TSentry.h"
#include "Common.h"

#include <sentry.h>
#include <sstream>

static size_t SentryUrlLen;

// compile-time length of a string/array
template <size_t N>
constexpr size_t ConstexprLength(char const (&)[N]) {
    return N - 1;
}

TSentry::TSentry() {
    if constexpr (ConstexprLength(SECRET_SENTRY_URL) == 0) {
        mValid = false;
    } else {
        mValid = true;
        sentry_options_t* options = sentry_options_new();
        sentry_options_set_dsn(options, SECRET_SENTRY_URL);
        sentry_options_set_debug(options, false); // needs to always be false
        sentry_options_set_symbolize_stacktraces(options, true);
        auto ReleaseString = "BeamMP-Server@" + Application::ServerVersion();
        sentry_options_set_release(options, ReleaseString.c_str());
        sentry_options_set_max_breadcrumbs(options, 10);
        sentry_init(options);
    }
}

TSentry::~TSentry() {
    if (mValid) {
        sentry_close();
    }
}

void TSentry::PrintWelcome() {
    if (mValid) {
        info("Sentry started");
        debug("Sentry URL is length " + std::to_string(SentryUrlLen));
    } else {
        info("Sentry disabled in unofficial build");
    }
}

void TSentry::SetupUser() {
    if (!mValid) {
        return;
    }
    sentry_value_t user = sentry_value_new_object();
    if (Application::Settings.Key.size() == 36) {
        sentry_value_set_by_key(user, "id", sentry_value_new_string(Application::Settings.Key.c_str()));
    } else {
        sentry_value_set_by_key(user, "id", sentry_value_new_string("unauthenticated"));
    }
    sentry_set_user(user);
}

void TSentry::Log(SentryLevel level, const std::string& logger, const std::string& text) {
    if (!mValid) {
        return;
    }
    SetContext("threads", { { "thread-name", ThreadName(true) } });
    auto Msg = sentry_value_new_message_event(sentry_level_t(level), logger.c_str(), text.c_str());
    sentry_capture_event(Msg);
    sentry_remove_transaction();
}

void TSentry::LogError(const std::string& text, const std::string& file, const std::string& line) {
    if (!mValid) {
        return;
    }
    SetTransaction(file + ":" + line);
    Log(SentryLevel::Error, "default", file + ": " + text);
}

void TSentry::SetContext(const std::string& context_name, const std::unordered_map<std::string, std::string>& map) {
    if (!mValid) {
        return;
    }
    auto ctx = sentry_value_new_object();
    for (const auto& pair : map) {
        std::string key = pair.first;
        if (key == "type") {
            // `type` is reserved
            key = "_type";
        }
        sentry_value_set_by_key(ctx, key.c_str(), sentry_value_new_string(pair.second.c_str()));
    }
    sentry_set_context(context_name.c_str(), ctx);
}

void TSentry::LogException(const std::exception& e, const std::string& file, const std::string& line) {
    if (!mValid) {
        return;
    }
    SetTransaction(file + ":" + line);
    Log(SentryLevel::Fatal, "exceptions", std::string(e.what()) + " @ " + file + ":" + line);
}

void TSentry::LogAssert(const std::string& condition_string, const std::string& file, const std::string& line, const std::string& function) {
    if (!mValid) {
        return;
    }
    SetTransaction(file + ":" + line + ":" + function);
    std::stringstream ss;
    ss << "\"" << condition_string << "\" failed @ " << file << ":" << line;
    Log(SentryLevel::Fatal, "asserts", ss.str());
}

void TSentry::AddErrorBreadcrumb(const std::string& msg, const std::string& file, const std::string& line) {
    if (!mValid) {
        return;
    }
    auto crumb = sentry_value_new_breadcrumb("default", (msg + " @ " + file + ":" + line).c_str());
    sentry_value_set_by_key(crumb, "level", sentry_value_new_string("error"));
    sentry_add_breadcrumb(crumb);
}

void TSentry::SetTransaction(const std::string& id) {
    if (!mValid) {
        return;
    }
    sentry_set_transaction(id.c_str());
}

std::unique_lock<std::mutex> TSentry::CreateExclusiveContext() {
    return std::unique_lock<std::mutex>(mMutex);
}
