#include "Sentry.h"
#include "Common.h"

TSentry::TSentry(const std::string& SentryUrl) {
    if (SentryUrl.empty()) {
        mValid = false;
    } else {
        mValid = true;
        sentry_options_t* options = sentry_options_new();
        sentry_options_set_dsn(options, SentryUrl.c_str());
        auto ReleaseString = "BeamMP-Server@" + Application::ServerVersion();
        sentry_options_set_release(options, ReleaseString.c_str());
        sentry_init(options);
    }
}

TSentry::~TSentry() {
    if (mValid) {
        sentry_close();
    }
}

void TSentry::Log(sentry_level_t level, const std::string& logger, const std::string& text) {
    sentry_capture_event(sentry_value_new_message_event(level, logger.c_str(), text.c_str()));
}

void TSentry::LogException(const std::exception& e, const std::string& file, const std::string& line) {
    Log(SENTRY_LEVEL_ERROR, "exceptions", std::string(e.what()) + " @ " + file + ":" + line);
}

void TSentry::AddErrorBreadcrumb(const std::string& msg, const std::string& file, const std::string& line) {
    auto crumb = sentry_value_new_breadcrumb("default", (msg + " @ " + file + ":" + line).c_str());
    sentry_value_set_by_key(crumb, "level", sentry_value_new_string("error"));
    sentry_add_breadcrumb(crumb);
}
