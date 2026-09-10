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

#include <regex>

void RegionHandler::TopLevelDomainFailed()
{
    static bool isDeveloperRegion = Application::Settings.getAsString(Settings::Key::General_Region) == "Developer";
    if (!isDeveloperRegion) {
        beammp_info("Top level domain of " + mValidTLDs[mRegionIndex % mValidTLDs.size()] + " didn't respond correctly , changing domain to " + mValidTLDs[(mRegionIndex + 1) % mValidTLDs.size()]);
    }
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

std::string RegionHandler::RedirectURL(const std::string &URL)
{
    std::regex link_pattern(R"(^(https:\/\/.*)beammp\.com(\/.*)?$)");
    std::smatch link_match;
    if (std::regex_search(URL, link_match, link_pattern) && link_match.position() == 0) {
        //TLD matched beammp.com
        std::string before = link_match[1].str();                     // "https://..." up to beammp
        std::string after  = link_match[2].matched ? link_match[2].str() : ""; // "/path" or ""
        return before + RegionToTopLevelDomain() + after;
    }
    return URL; //if it didn't match, just return the unmodified URL
}
