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

#include "RegionHandler.h"
#include "Common.h"

void RegionHandler::TopLevelDomainFailed()
{
    beammp_info("Top level domain of " + mValidTLDs[mRegionIndex % mValidTLDs.size()] + " didn't respond correctly , changing domain to " + mValidTLDs[(mRegionIndex + 1) % mValidTLDs.size()]);
    mRegionIndex++;
}

std::string RegionHandler::RegionToTopLevelDomain()
{
    static bool isDeveloperRegion = Application::Settings.getAsString(Settings::Key::General_Region) == "Developer";
    if (isDeveloperRegion) {
        return "beammp.dev";
    }
    return mValidTLDs[mRegionIndex % mValidTLDs.size()]; // Global
}
