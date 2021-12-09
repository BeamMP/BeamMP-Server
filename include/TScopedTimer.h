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
    auto GetElapsedTime() const {
        auto EndTime = std::chrono::high_resolution_clock::now();
        auto Delta = EndTime - mStartTime;
        size_t TimeDelta = Delta / std::chrono::milliseconds(1);
        return TimeDelta;
    }

    std::function<void(size_t /* time_ms */)> OnDestroy { nullptr };

private:
    std::chrono::high_resolution_clock::time_point mStartTime;
    std::string Name;
};
