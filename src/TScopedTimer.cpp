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

#include "TScopedTimer.h"
#include "Common.h"

TScopedTimer::TScopedTimer()
    : mStartTime(std::chrono::high_resolution_clock::now()) {
}

TScopedTimer::TScopedTimer(const std::string& mName)
    : mStartTime(std::chrono::high_resolution_clock::now())
    , Name(mName) {
}

TScopedTimer::TScopedTimer(std::function<void(size_t)> OnDestroy)
    : OnDestroy(OnDestroy)
    , mStartTime(std::chrono::high_resolution_clock::now()) {
}

TScopedTimer::~TScopedTimer() {
    auto EndTime = std::chrono::high_resolution_clock::now();
    auto Delta = EndTime - mStartTime;
    size_t TimeDelta = Delta / std::chrono::milliseconds(1);
    if (OnDestroy) {
        OnDestroy(TimeDelta);
    } else {
        beammp_info("Scoped timer: \"" + Name + "\" took " + std::to_string(TimeDelta) + "ms ");
    }
}
