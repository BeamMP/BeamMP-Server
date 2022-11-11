#pragma once

#include <chrono>
#include <functional>
#include <string>

#include "Common.h"

class TScopedTimer {
public:
    TScopedTimer();
    TScopedTimer(const std::string& Name);
    TScopedTimer(std::function<void(size_t)> OnDestroy);
    ~TScopedTimer();
    auto GetElapsedTime() const {
        auto EndTime = TimeType::now();
        auto Delta = EndTime - mStartTime;
        size_t TimeDelta = Delta / std::chrono::milliseconds(1);
        return TimeDelta;
    }

    std::function<void(size_t /* time_ms */)> OnDestroy { nullptr };

private:
    TimeType::time_point mStartTime;
    std::string Name;
};
