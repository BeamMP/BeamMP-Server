#include "Common.h"
#include "TConsole.h"

std::unique_ptr<TConsole> Application::mConsole = std::make_unique<TConsole>();

void Application::RegisterShutdownHandler(const TShutdownHandler& Handler) {
    std::unique_lock Lock(mShutdownHandlersMutex);
    if (Handler) {
        mShutdownHandlers.push_back(Handler);
    }
}

void Application::GracefullyShutdown() {
    std::unique_lock Lock(mShutdownHandlersMutex);
    for (auto& Handler : mShutdownHandlers) {
        Handler();
    }
}
