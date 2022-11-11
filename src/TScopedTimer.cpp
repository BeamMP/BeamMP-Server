#include "TScopedTimer.h"
#include "Common.h"

TScopedTimer::TScopedTimer()
    : mStartTime(std::chrono::system_clock::now()) {
}

TScopedTimer::TScopedTimer(const std::string& mName)
    : mStartTime(std::chrono::system_clock::now())
    , Name(mName) {
}

TScopedTimer::TScopedTimer(std::function<void(size_t)> OnDestroy)
    : OnDestroy(OnDestroy)
    , mStartTime(std::chrono::system_clock::now()) {
}

TScopedTimer::~TScopedTimer() {
    auto EndTime = std::chrono::system_clock::now();
    auto Delta = EndTime - mStartTime;
    size_t TimeDelta = Delta / std::chrono::milliseconds(1);
    if (OnDestroy) {
        OnDestroy(TimeDelta);
    } else {
        beammp_info("Scoped timer: \"" + Name + "\" took " + std::to_string(TimeDelta) + "ms ");
    }
}
