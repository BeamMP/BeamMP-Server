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

#include <string>

class TVehicleData final {
public:
    TVehicleData(int ID, std::string Data);
    ~TVehicleData();
    // We cannot delete this, since vector needs to be able to copy when it resizes.
    // Deleting this causes some wacky template errors which are hard to decipher,
    // and end up making no sense, so let's just leave the copy ctor.
    // TVehicleData(const TVehicleData&) = delete;

    [[nodiscard]] bool IsInvalid() const { return mID == -1; }
    [[nodiscard]] int ID() const { return mID; }

    [[nodiscard]] std::string Data() const { return mData; }
    void SetData(const std::string& Data) { mData = Data; }

    bool operator==(const TVehicleData& v) const { return mID == v.mID; }

private:
    int mID { -1 };
    std::string mData;
};

// TODO: unused now, remove?
namespace std {
template <>
struct hash<TVehicleData> {
    std::size_t operator()(const TVehicleData& s) const noexcept {
        return s.ID();
    }
};
}
