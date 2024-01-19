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

#include <Common.h>
#include <IThreaded.h>
#include <filesystem>
#include <string>
#include <unordered_map>

#if defined(BEAMMP_LINUX)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif
#include <httplib.h>
#if defined(BEAMMP_LINUX)
#pragma GCC diagnostic pop
#endif

namespace fs = std::filesystem;

namespace Http {
std::string GET(const std::string& host, int port, const std::string& target, unsigned int* status = nullptr);
std::string POST(const std::string& host, int port, const std::string& target, const std::string& body, const std::string& ContentType, unsigned int* status = nullptr, const httplib::Headers& headers = {});
namespace Status {
    std::string ToString(int code);
}
const std::string ErrorString = "-1";

namespace Server {
    class THttpServerInstance {
    public:
        THttpServerInstance();

    protected:
        void operator()();

    private:
        std::thread mThread;
    };
}
}
