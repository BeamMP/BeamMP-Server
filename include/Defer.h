#pragma once

#include <functional>

template <typename FnT>
class Defer final {
public:
    Defer(FnT fn)
        : mFunction([&fn] { (void)fn(); }) { }
    ~Defer() {
        if (mFunction) {
            mFunction();
        }
    }

private:
    std::function<void()> mFunction;
};
