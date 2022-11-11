#include "TScopedTimer.h"
#include "Common.h"

TScopedTimer::TScopedTimer()
    : mStartTime(TimeType::now()) {
}

TScopedTimer::TScopedTimer(const std::string& mName)
    : mStartTime(TimeType::now())
    , Name(mName) {
}

TScopedTimer::TScopedTimer(std::function<void(size_t)> OnDestroy)
    : OnDestroy(OnDestroy)
    , mStartTime(TimeType::now()) {
}

TScopedTimer::~TScopedTimer() {
    auto EndTime = TimeType::now();
    auto Delta = EndTime - mStartTime;
    size_t TimeDelta = Delta / std::chrono::milliseconds(1);
    if (OnDestroy) {
        OnDestroy(TimeDelta);
    } else {
        beammp_info("Scoped timer: \"" + Name + "\" took " + std::to_string(TimeDelta) + "ms ");
    }
}
