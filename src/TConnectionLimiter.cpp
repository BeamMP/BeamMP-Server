// BeamMP, the BeamNG.drive multiplayer mod.
// Copyright (C) 2026 BeamMP Ltd., BeamMP team and contributors.
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

#include "TConnectionLimiter.h"
#include <mutex>
#include <optional>
#include "Common.h"

TConnectionLimiter::TConnectionLimiter(size_t maxPerIp, size_t maxGlobal)
    : mMaxPerIp(maxPerIp)
    , mMaxGlobal(maxGlobal) {
    if (maxPerIp == 0) {
        beammp_errorf("Max connections count per IP is set to zero; the server would reject ALL connections");
        throw std::runtime_error("Invalid maximum connections per IP setting");
    }
    if (maxGlobal == 0) {
        beammp_errorf("Max connection count is set to zero; the server would reject ALL connections");
        throw std::runtime_error("Invalid maximum connections setting");
    }
}

std::optional<TConnectionLimiter::TGuard> TConnectionLimiter::TryAcquire(const std::string& ip) {
    std::unique_lock Lock { mMutex };
    if (mGlobal >= mMaxGlobal) return std::nullopt;
    // `It` is the inserted element (so if insertion worked, its 0), or its the element that
    // was already there, in which case we must check the ip connection limit
    auto [It, _] = mPerIp.try_emplace(ip, 0);
    if (It->second >= mMaxPerIp) {
        return std::nullopt;
    }
    // now increment the counter finally
    ++It->second;
    ++mGlobal;
    // RAII guard will drop the count once destructed
    return TGuard(this, ip);
}

TConnectionLimiter::TStats TConnectionLimiter::GetStats() {
    std::unique_lock Lock { mMutex };
    TStats Stats;
    Stats.CurrentGlobal = mGlobal;
    Stats.MaxGlobal = mMaxGlobal;
    Stats.ActiveIpBuckets = mPerIp.size();
    Stats.MaxPerIp = mMaxPerIp;
    for (const auto& [_, Count] : mPerIp) {
        if (Count > Stats.CurrentMaxPerIp) {
            Stats.CurrentMaxPerIp = Count;
        }
        if (Count >= mMaxPerIp) {
            ++Stats.SaturatedIpBuckets;
        }
    }
    return Stats;
}

TConnectionLimiter::TGuard::TGuard(TConnectionLimiter* owner, std::string ip)
    : mOwner(owner)
    , mIp(std::move(ip)) {
    beammp_debugf("Acquired connection guard for {}", mIp);
}

TConnectionLimiter::TGuard& TConnectionLimiter::TGuard::operator=(TGuard&& other) noexcept {
    // identity check
    if (this != &other) {
        Release();
        mOwner = other.mOwner;
        mIp = std::move(other.mIp);
        other.mOwner = nullptr;
        // setting this so its obvious when this happens, instead of being UB or empty string
        other.mIp = "<moved-from>";
    }
    return *this;
}

void TConnectionLimiter::TGuard::Release() {
    if (mOwner) {
        mOwner->Release(mIp);
        mOwner = nullptr;
        beammp_debugf("Released connection guard for {}", mIp);
    }
}
