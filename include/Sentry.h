#ifndef SENTRY_H
#define SENTRY_H

#include <string>

// singleton, dont make this twice
class Sentry final {
public:
    Sentry(const std::string& SentryUrl);
    ~Sentry();
private:
};

#endif // SENTRY_H
