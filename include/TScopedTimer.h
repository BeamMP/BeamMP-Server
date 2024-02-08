// BeamMP, the BeamNG.drive multiplayer mod.
// Copyright (C) 2024 BeamMP Ltd., BeamMP team and contributors.
//
// BeamMP Ltd. can be contacted by electronic mail via contact@beammp.com.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

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
