#pragma once

/// @file
/// The Value.h file describes a collection of wrapper types for use in
/// cross-plugin communication and similar. These wrapper types are
/// typically not zero-cost, so be careful and use these sparigly.
///
/// Base visitors, such as ValueToStringVisitor, should be declared
/// here also.

#include "Error.h"
#include "HashMap.h"
#include "boost/variant/variant_fwd.hpp"
#include <boost/json.hpp>
#include <boost/variant.hpp>
#include <boost/variant/variant.hpp>
#include <ostream>
#include <sol/forward.hpp>
#include <string>
#include <vector>

/// Dynamic array, can resize.
template<typename T>
using Array = std::vector<T>;

/// Null value, for use in Value.
struct Null {
    /// Makes a null value. It's an identity value,
    /// so its existance is the value.
    explicit Null() { }
};

/// Formats "null".
std::ostream& operator<<(std::ostream& os, const Null&);

/// Contains a boolean value, for use in Value,
/// as booleans will be implicitly converted to int.
struct Bool {
    /// Construct a bool from a boolean.
    explicit Bool(bool b_)
        : b(b_) { }
    /// Contained value.
    bool b;
    /// Implicit conversion to bool, because it's expected to work this way.
    operator bool() const { return b; }
};

template<typename T>
struct Tuple final : public Array<T> {
    using Array<T>::Array;
};

/// Formats to "true" or "false".
std::ostream& operator<<(std::ostream& os, const Bool&);

/// The Value type is a recursively defined variant, which allows
/// passing a single value with any of a selection of types, including
/// the possibility to pass hashmaps of hashmaps of hashmaps of types (and so on).
///
/// In common pseudo-C++, this would be written as:
///
/// \code{.cpp}
/// using Value = variant<string, int, double, HashMap<string, Value>;
/// //                                                         ^^^^^
/// \endcode
/// Note the `^^^` annotated recursion. This isn't directly possible in C++,
/// so we use boost's recursive variants for this. Documentation is here
/// https://www.boost.org/doc/libs/1_82_0/doc/html/variant/tutorial.html#variant.tutorial.recursive
///
/// The use-case of a Value is to represent almost any primitive-ish type we may get from, or
/// may want to pass to, a Plugin.
///
/// For example, a table of key-value pairs, or a table of tables, or just a string, or a float, could all
/// be represented by this.
///
/// See the abstract template class ValueVisitor for how to access this with the
/// visitor pattern.
using Value = boost::make_recursive_variant<
    std::string,
    int64_t,
    double,
    Null,
    Array<boost::recursive_variant_>,
    HashMap<std::string, boost::recursive_variant_>,
    Bool,
    Tuple<boost::recursive_variant_>>::type;

// the following VALUE_TYPE_* variables are used mostly for
// unit-tests and code that can't use visitors.

/// Index of string in Value
[[maybe_unused]] constexpr int VALUE_TYPE_IDX_STRING = 0;
/// Index of int in Value
[[maybe_unused]] constexpr int VALUE_TYPE_IDX_INT = 1;
/// Index of double in Value
[[maybe_unused]] constexpr int VALUE_TYPE_IDX_DOUBLE = 2;
/// Index of null in Value
[[maybe_unused]] constexpr int VALUE_TYPE_IDX_NULL = 3;
/// Index of array in Value
[[maybe_unused]] constexpr int VALUE_TYPE_IDX_ARRAY = 4;
/// Index of hashmap in Value
[[maybe_unused]] constexpr int VALUE_TYPE_IDX_HASHMAP = 5;
/// Index of bool in Value
[[maybe_unused]] constexpr int VALUE_TYPE_IDX_BOOL = 6;
/// Index of tuple in Value
[[maybe_unused]] constexpr int VALUE_TYPE_IDX_TUPLE = 7;

/// A handy typedef for the recursive HashMap type inside a Value.
/// You may have to use this in order to make the compiler understand
/// what kind of value (a hash map) you are constructing.
using ValueHashMap = HashMap<std::string, Value>;
/// A handy typedef for the recursive Array type inside a Value.
/// You may have to use this in order to make the compiler understand
/// what kind of value (an array) you are constructing.
using ValueArray = Array<Value>;
/// A handy dandy typedef for using a tuple of values.
using ValueTuple = Tuple<Value>;

/// The ValueVisitor class is an abstract interface which allows the implementation
/// to easily construct a visitor for a Value object.
///
/// A Value object is a recursive variant class, and as such it's not simple to access
/// (no variants are really trivial to access). The visitor pattern gives us a type-safe
/// way to access such a variant, and the boost::static_visitor pattern does so in a
/// pretty concise way.
///
/// An example use is the ValueToStringVisitor.
template<typename ResultT>
class ValueVisitor : public boost::static_visitor<ResultT> {
public:
    /// Needs to be default-constructible for the standard use case (see example above).
    ValueVisitor() = default;
    /// Cannot be copied.
    ValueVisitor(const ValueVisitor&) = delete;
    /// Cannot be copied.
    ValueVisitor& operator=(const ValueVisitor&) = delete;
    /// Virtual destructor is needed for virtual classes.
    virtual ~ValueVisitor() = default;

    /// ResultT from string.
    virtual ResultT operator()(const std::string& str) const = 0;
    /// ResultT from integer.
    virtual ResultT operator()(int64_t i) const = 0;
    /// ResultT from float.
    virtual ResultT operator()(double d) const = 0;
    /// ResultT from null.
    virtual ResultT operator()(Null null) const = 0;
    /// ResultT from boolean.
    virtual ResultT operator()(Bool b) const = 0;
    /// ResultT from array of values (must recursively visit).
    virtual ResultT operator()(const ValueArray& array) const = 0;
    /// ResultT from tuple of values (must recursively visit).
    virtual ResultT operator()(const ValueTuple& array) const = 0;
    /// ResultT from hashmap of values (must recursively visit).
    virtual ResultT operator()(const HashMap<std::string, Value>& map) const = 0;
};

/// The ValueToStringVisitor class implements a visitor for a Value which
/// turns it into a human-readable string.
///
/// Example
/// \code{.cpp}
/// #include <boost/variant.hpp>
///
/// Value value = ...;
///
/// std::string str = boost::apply_visitor(ValueToStringVisitor(), value);
/// //                                     ^--------------------^  ^---^
/// //                                       default ctor            |
/// //                                                         value to visit
/// \endcode
class ValueToStringVisitor : public ValueVisitor<std::string> {
public:
    /// Flag used to specify behavior of ValueToStringVisitor.
    enum Flag {
        /// No options
        NONE = 0,
        /// Quote strings, `value` becomes `"value"`.
        QUOTE_STRINGS = 0b1,
    };

    /// Constructs a ValueToStringVisitor with options.
    /// With flags you can change, for example, whether strings should be quoted
    /// when they standalone.
    /// Depth is used by recursion, ignore it.
    explicit ValueToStringVisitor(Flag flags = QUOTE_STRINGS, int depth = 1);
    /// Returns the same string, possibly quoted (depends on flags).
    std::string operator()(const std::string& str) const;
    /// Uses fmt::format() to stringify the integer.
    std::string operator()(int64_t i) const;
    /// Uses fmt::format() to stringify the double.
    std::string operator()(double d) const;
    /// Returns "null".
    std::string operator()(Null null) const;
    /// Returns "true" or "false".
    std::string operator()(Bool b) const;
    /// Returns an object of format [ value, value, value ].
    /// Recursively visits the elements of the array.
    std::string operator()(const ValueArray& array) const;
    /// Returns a tuple of format ( value, value, value ).
    /// Recursively visits the elements of the array.
    std::string operator()(const ValueTuple& array) const;
    /// Returns an object of format { key: value, key: value }.
    /// Recursively visits the elements of the map.
    std::string operator()(const HashMap<std::string, Value>& map) const;

private:
    /// Whether to quote strings before output.
    bool m_quote_strings;
    /// How many 2-space "tabs" to use - used by recursion.
    int m_depth;
};

/// The ValueToJsonVisitor class is used to convert a Value into
/// a boost::json object.
class ValueToJsonVisitor : public ValueVisitor<boost::json::value> {
public:
    /// Converts to json string.
    boost::json::value operator()(const std::string& str) const;
    /// Converts to json integer.
    boost::json::value operator()(int64_t i) const;
    /// Converts to json float.
    boost::json::value operator()(double d) const;
    /// Converts to empty json value.
    boost::json::value operator()(Null null) const;
    /// Converts to json boolean.
    boost::json::value operator()(Bool b) const;
    /// Converts to json array.
    boost::json::value operator()(const ValueArray& array) const;
    /// Converts to json array (because tuples don't exist).
    boost::json::value operator()(const ValueTuple& array) const;
    /// Converts to json object.
    boost::json::value operator()(const HashMap<std::string, Value>& map) const;
};

/// The ValueToLuaVisitor class is used to convert a Value into a
/// sol object.
class ValueToLuaVisitor : public ValueVisitor<sol::object> {
public:
    /// ValueToLuaVisitor needs a sol state in order to construct objects.
    ValueToLuaVisitor(sol::state& state);

    sol::object operator()(const std::string& str) const;
    sol::object operator()(int64_t i) const;
    sol::object operator()(double d) const;
    sol::object operator()(Null null) const;
    sol::object operator()(Bool b) const;
    sol::object operator()(const ValueArray& array) const;
    sol::object operator()(const ValueTuple& array) const;
    sol::object operator()(const HashMap<std::string, Value>& map) const;

private:
    sol::state& m_state;
};

/// This function converts from a lua (sol) wrapped value into a beammp value, for use in C++.
///
/// Value is a type which can be passed around between threads, and has no external dependencies.
/// Sol values are not like that, as they are references to stack indices in lua, and similar.
///
/// This function is also used to print values, by first converting them to a Value, then using a
/// ValueToStringVisitor.
///
/// The second argument is a provider for values which the function can't convert.
/// "invalid provider" means "provider of values for invalid sol values". If nullptr, then
/// any invalid value (such as a function) will be resolved to an error instead and the function will
/// fail.
Result<Value> sol_obj_to_value(const sol::object&, const std::function<Result<Value>(const sol::object&)>& invalid_provider = nullptr, size_t max_depth = 50);

