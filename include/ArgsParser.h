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

#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/*
 * Allows syntax:
 *  --help              : long flags
 *  --path=/home/lion   : long assignments
 */
class ArgsParser {
public:
    enum Flags : int {
        NONE = 0,
        REQUIRED = 1, // argument is required
        HAS_VALUE = 2, // argument must have a value
    };

    ArgsParser() = default;

    /**
     * Parse all arguments provided, if they have been register.
     */
    void Parse(const std::vector<std::string_view>& ArgList);
    // prints errors if any errors occurred, in that case also returns false
    bool Verify();
    void RegisterArgument(std::vector<std::string>&& ArgumentNames, int Flags);
    // pass all possible names for this argument (short, long, etc)
    bool FoundArgument(const std::vector<std::string>& Names);
    std::optional<std::string> GetValueOfArgument(const std::vector<std::string>& Names);

private:
    
    /**
     * Register an argument with a value.
     */
    void ConsumeLongAssignment(const std::string& Arg);
    void ConsumeLongFlag(const std::string& Arg);
    /**
     * return if the argument asked has been registered previously.
     */
    bool IsRegistered(const std::string& Name);

    struct Argument {
        std::string Name;
        std::optional<std::string> Value;
    };

    struct RegisteredArgument {
        std::vector<std::string> Names;
        int Flags;
    };

    std::vector<RegisteredArgument> mRegisteredArguments;
    std::vector<Argument> mFoundArgs;
};
