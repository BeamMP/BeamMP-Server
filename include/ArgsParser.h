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

    void Parse(const std::vector<std::string_view>& ArgList);
    // prints errors if any errors occurred, in that case also returns false
    bool Verify();
    void RegisterArgument(std::vector<std::string>&& ArgumentNames, int Flags);
    // pass all possible names for this argument (short, long, etc)
    bool FoundArgument(const std::vector<std::string>& Names);
    std::optional<std::string> GetValueOfArgument(const std::vector<std::string>& Names);

private:
    void ConsumeLongAssignment(const std::string& Arg);
    void ConsumeLongFlag(const std::string& Arg);
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
