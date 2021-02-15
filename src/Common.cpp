#include "Common.h"
#include "TConsole.h"

std::unique_ptr<TConsole> Application::_Console = std::make_unique<TConsole>();

void Application::RegisterShutdownHandler(const TShutdownHandler& Handler) {
    std::unique_lock Lock(_ShutdownHandlersMutex);
    if (Handler) {
        _ShutdownHandlers.push_back(Handler);
    }
}

void Application::GracefullyShutdown() {
    std::unique_lock Lock(_ShutdownHandlersMutex);
    for (auto& Handler : _ShutdownHandlers) {
        Handler();
    }
}
