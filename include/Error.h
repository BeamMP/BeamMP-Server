#pragma once

#include <fmt/core.h>
#include <optional>
#include <string>

/// The Error class represents an error or the absence of an
/// error. It behaves like a bool, depending on context.
///
/// The idea is to use this class to pass around errors, together with
/// [[nodiscard]], in order to make errors displayable for the users and
/// to give errors some context. The only way to construct an error is to come
/// up with an error message with this class, so this is an attempt to enforce
/// this.
///
/// A default constructed Error means "no error" / "success", while
/// the only available non-default constructor is one which takes any format.
/// For example:
///
/// \code{.cpp}
/// Error myfunc() {
///     if (ok) {
///         return {}; // no error
///     } else {
///         return Error("Something went wrong: {}", 42); // error
///     }
/// }
///
/// // ... handling:
///
/// Error err = myfunc();
/// if (err) {
///     // error occurred
///     l::error("Error running myfunc: {}", err.error);
/// } else {
///     // ok
/// }
/// \endcode
struct Error {
    /// Constructs a "non-error" / empty error, which is not considered
    /// to be an error. Use this as the "no error occurred" return value.
    Error() = default;
    /// Constructs an error with a message. Accepts fmt::format() arguments.
    ///
    /// Example:
    ///
    /// \code{.cpp}
    /// // format with fmt (automatically), all arguments are forwarded to fmt::format
    /// return Error("failed to open '{}': {}", file, error);
    /// // or just as a constexpr string
    /// return Error("failed to open file");
    /// \endcode
    template<typename... Args>
    Error(fmt::format_string<Args...> s, Args&&... args)
        : is_error(true)
        , error(fmt::format(s, std::forward<Args>(args)...)) { }

    /// Whether this error represents an error (true) or success (false).
    /// Use operator bool() instead of reading this if possible.
    bool is_error { false };
    /// The error message. Is a valid string even if is_error is false, but will
    /// be "Success".
    std::string error { "Success" };

    /// Implicit conversion to boolean.
    /// True if this Error contains an error, false if not.
    operator bool() const { return is_error; }
};

// TODO: Add docs

template<typename T>
struct Result {
    /// Constructs an error-value result.
    /// Currently, you may have to force this by passing a second
    /// empty string argument.
    template<typename... Args>
    Result(fmt::format_string<Args...> s, Args&&... args)
        : is_error(true)
        , error(fmt::format(s, std::forward<Args>(args)...)) { }

    /// Constructs a value-result via an explicit type.
    template<typename S>
    Result(S&& value)
        : result(std::move(value)) {
    }

    /// Constructs a value-result via an explicit type.
    template<typename S>
    explicit Result(const S& value)
        : result(value) {
    }

    /// Constructs a value-result via an implicit conversion.
    Result(T&& value)
        : result(std::move(value)) {
    }

    /// Constructs a value-result via an implicit conversion.
    explicit Result(const T& value)
        : result(value) {
    }

    /// Converts to bool in context. If it has an error, its considered "not a result",
    /// so it returns true only if a value is contained.
    operator bool() const { return !is_error; }

    /// Accesses the value contained by moving it out.
    T&& move() { return std::move(result.value()); }
    /// Accesses the value contained by const reference.
    const T& value() const { return result.value(); }

    /// Holds the optional result value. On error, is nullopt.
    std::optional<T> result;
    /// Whether this result holds an error.
    bool is_error { false };
    /// Error message.
    std::string error { "Success" };
};

