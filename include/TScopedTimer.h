#pragma once

#include <chrono>
#include <functional>
#include <string>

class TScopedTimer {
public:
    TScopedTimer();
    TScopedTimer(const std::string& Name);
    TScopedTimer(std::function<void(size_t)> OnDestroy);
    ~TScopedTimer();

    std::function<void(size_t /* time_ms */)> OnDestroy { nullptr };

private:
    std::chrono::high_resolution_clock::time_point mStartTime;
    std::string Name;
};
