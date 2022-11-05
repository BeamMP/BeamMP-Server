#include "ArgsParser.h"
#include "Common.h"
#include <algorithm>
#include <doctest/doctest.h>

void ArgsParser::Parse(const std::vector<std::string_view>& ArgList) {
    for (const auto& Arg : ArgList) {
        if (Arg.size() > 2 && Arg.substr(0, 2) == "--") {
            // long arg
            if (Arg.find("=") != Arg.npos) {
                ConsumeLongAssignment(std::string(Arg));
            } else {
                ConsumeLongFlag(std::string(Arg));
            }
        } else {
            fmt::print(stderr, "Error parsing commandline arguments: Supplied argument '{}' is not a valid argument and was ignored.", Arg);
        }
    }
}

bool ArgsParser::Verify() {
    bool Ok = true;
    for (const auto& RegisteredArg : mRegisteredArguments) {
        if (RegisteredArg.Flags & Flags::REQUIRED && !FoundArgument(RegisteredArg.Names)) {
            fmt::print(stderr, "Error in commandline arguments: Argument '{}' is required but wasn't found.", RegisteredArg.Names.at(0));
            Ok = false;
            continue;
        } else if (FoundArgument(RegisteredArg.Names)) {
            if (RegisteredArg.Flags & Flags::HAS_VALUE) {
                if (!GetValueOfArgument(RegisteredArg.Names).has_value()) {
                    fmt::print(stderr, "Error in commandline arguments: Argument '" + std::string(RegisteredArg.Names.at(0)) + "' expects a value, but no value was given.");
                    Ok = false;
                }
            } else if (GetValueOfArgument(RegisteredArg.Names).has_value()) {
                fmt::print(stderr, "Error in commandline arguments: Argument '" + std::string(RegisteredArg.Names.at(0)) + "' does not expect a value, but one was given.");
                Ok = false;
            }
        }
    }
    return Ok;
}

void ArgsParser::RegisterArgument(std::vector<std::string>&& ArgumentNames, int Flags) {
    mRegisteredArguments.push_back({ ArgumentNames, Flags });
}

bool ArgsParser::FoundArgument(const std::vector<std::string>& Names) {
    // if any of the found args match any of the names
    return std::any_of(mFoundArgs.begin(), mFoundArgs.end(),
        [&Names](const Argument& Arg) -> bool {
            // if any of the names match this arg's name
            return std::any_of(Names.begin(), Names.end(), [&Arg](const std::string& Name) -> bool {
                return Arg.Name == Name;
            });
        });
}

std::optional<std::string> ArgsParser::GetValueOfArgument(const std::vector<std::string>& Names) {
    // finds an entry which has a name that is any of the names in 'Names'
    auto Found = std::find_if(mFoundArgs.begin(), mFoundArgs.end(), [&Names](const Argument& Arg) -> bool {
        return std::any_of(Names.begin(), Names.end(), [&Arg](const std::string_view& Name) -> bool {
            return Arg.Name == Name;
        });
    });
    if (Found != mFoundArgs.end()) {
        // found
        return Found->Value;
    } else {
        return std::nullopt;
    }
}

bool ArgsParser::IsRegistered(const std::string& Name) {
    return std::any_of(mRegisteredArguments.begin(), mRegisteredArguments.end(), [&Name](const RegisteredArgument& Arg) {
        auto Iter = std::find(Arg.Names.begin(), Arg.Names.end(), Name);
        return Iter != Arg.Names.end();
    });
}

void ArgsParser::ConsumeLongAssignment(const std::string& Arg) {
    auto Value = Arg.substr(Arg.rfind("=") + 1);
    auto Name = Arg.substr(2, Arg.rfind("=") - 2);
    if (!IsRegistered(Name)) {
        fmt::print(stdout, "Argument '" + Name + "' was supplied but isn't a known argument, so it is likely being ignored.");
    }
    mFoundArgs.push_back({ Name, Value });
}

void ArgsParser::ConsumeLongFlag(const std::string& Arg) {
    auto Name = Arg.substr(2, Arg.rfind("=") - 2);
    mFoundArgs.push_back({ Name, std::nullopt });
    if (!IsRegistered(Name)) {
        fmt::print(stdout, "Argument '" + Name + "' was supplied but isn't a known argument, so it is likely being ignored.");
    }
}

TEST_CASE("ArgsParser") {
    ArgsParser parser;

    SUBCASE("Simple args") {
        parser.RegisterArgument({ "a" }, ArgsParser::Flags::NONE);
        parser.RegisterArgument({ "hello" }, ArgsParser::Flags::NONE);
        parser.Parse({ "--a", "--hello" });
        CHECK(parser.Verify());
        CHECK(parser.FoundArgument({ "a" }));
        CHECK(parser.FoundArgument({ "hello" }));
        CHECK(parser.FoundArgument({ "a", "hello" }));
        CHECK(!parser.FoundArgument({ "b" }));
        CHECK(!parser.FoundArgument({ "goodbye" }));
    }

    SUBCASE("No args") {
        parser.RegisterArgument({ "a" }, ArgsParser::Flags::NONE);
        parser.RegisterArgument({ "hello" }, ArgsParser::Flags::NONE);
        parser.Parse({});
        CHECK(parser.Verify());
        CHECK(!parser.FoundArgument({ "a" }));
        CHECK(!parser.FoundArgument({ "hello" }));
        CHECK(!parser.FoundArgument({ "a", "hello" }));
        CHECK(!parser.FoundArgument({ "b" }));
        CHECK(!parser.FoundArgument({ "goodbye" }));
        CHECK(!parser.FoundArgument({ "" }));
    }

    SUBCASE("Value args") {
        parser.RegisterArgument({ "a" }, ArgsParser::Flags::HAS_VALUE);
        parser.RegisterArgument({ "hello" }, ArgsParser::Flags::HAS_VALUE);
        parser.Parse({ "--a=5", "--hello=world" });
        CHECK(parser.Verify());
        REQUIRE(parser.FoundArgument({ "a" }));
        REQUIRE(parser.FoundArgument({ "hello" }));
        CHECK(parser.GetValueOfArgument({ "a" }).has_value());
        CHECK(parser.GetValueOfArgument({ "a" }).value() == "5");
        CHECK(parser.GetValueOfArgument({ "hello" }).has_value());
        CHECK(parser.GetValueOfArgument({ "hello" }).value() == "world");
    }

    SUBCASE("Mixed value & no-value args") {
        parser.RegisterArgument({ "a" }, ArgsParser::Flags::HAS_VALUE);
        parser.RegisterArgument({ "hello" }, ArgsParser::Flags::NONE);
        parser.Parse({ "--a=5", "--hello" });
        CHECK(parser.Verify());
        REQUIRE(parser.FoundArgument({ "a" }));
        REQUIRE(parser.FoundArgument({ "hello" }));
        CHECK(parser.GetValueOfArgument({ "a" }).has_value());
        CHECK(parser.GetValueOfArgument({ "a" }).value() == "5");
        CHECK(!parser.GetValueOfArgument({ "hello" }).has_value());
    }

    SUBCASE("Required args") {
        SUBCASE("Two required, two present") {
            parser.RegisterArgument({ "a" }, ArgsParser::Flags::REQUIRED);
            parser.RegisterArgument({ "hello" }, ArgsParser::Flags::REQUIRED);
            parser.Parse({ "--a", "--hello" });
            CHECK(parser.Verify());
        }
        SUBCASE("Two required, one present") {
            parser.RegisterArgument({ "a" }, ArgsParser::Flags::REQUIRED);
            parser.RegisterArgument({ "hello" }, ArgsParser::Flags::REQUIRED);
            parser.Parse({ "--a" });
            CHECK(!parser.Verify());
        }
        SUBCASE("Two required, none present") {
            parser.RegisterArgument({ "a" }, ArgsParser::Flags::REQUIRED);
            parser.RegisterArgument({ "hello" }, ArgsParser::Flags::REQUIRED);
            parser.Parse({ "--b" });
            CHECK(!parser.Verify());
        }
    }
}
