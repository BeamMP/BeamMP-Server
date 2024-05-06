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

#include "Env.h"
#include <optional>

std::optional<std::string> Env::Get(Env::Key key) {
    auto StrKey = ToString(key);
    auto Value = std::getenv(StrKey.data());
    if (!Value || std::string_view(Value).empty()) {
        return std::nullopt;
    }
    return Value;
}

std::string_view Env::ToString(Env::Key key) {
    switch (key) {
    case Key::PROVIDER_UPDATE_MESSAGE:
        return "BEAMMP_PROVIDER_UPDATE_MESSAGE";
        break;
    case Key::PROVIDER_DISABLE_CONFIG:
        return "BEAMMP_PROVIDER_DISABLE_CONFIG";
        break;
    case Key::PROVIDER_PORT_ENV:
        return "BEAMMP_PROVIDER_PORT_ENV";
        break;
    }
    return "";
}
