#include "Sentry.h"
#include "Common.h"

#include "sentry.h"

Sentry::Sentry(const std::string& SentryUrl) {
    sentry_options_t* options = sentry_options_new();
    sentry_options_set_dsn(options, SentryUrl.c_str());
    auto ReleaseString = "BeamMP-Server@" + Application::ServerVersion();
    sentry_options_set_release(options, ReleaseString.c_str());
    sentry_init(options);
}

Sentry::~Sentry() {
    sentry_close();
}
