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

// one of BEAMMP_{WINDOWS,LINUX,APPLE} will be set at the end of this

// clang-format off
#if !defined(BEAMMP_WINDOWS) && !defined(BEAMMP_UNIX) && !defined(BEAMMP_APPLE)
    #if defined(_WIN32) || defined(__CYGWIN__)
        #define BEAMMP_WINDOWS
    #elif defined(__linux__) || defined(__linux) || defined(linux) || defined(__unix__) || defined(__unix) || defined(unix)
        #define BEAMMP_LINUX
    #elif defined(__APPLE__) || defined(__MACH__)
        #define BEAMMP_APPLE
    #else
        #error "This platform is not known. Please define one of the above for your OS."
    #endif
#endif
// clang-format on
