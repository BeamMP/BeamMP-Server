#pragma once

/// @file
/// HashMap holds hash map implementations and typedefs.
///
/// The idea is that we can easily swap out the implementation
/// in case there is a performance or memory usage concern.

#include <boost/container/flat_map.hpp>
#include <boost/thread/synchronized_value.hpp>

/// A hash map to be used for any kind of small number of key-value pairs.
/// Iterators and pointers may be invalidated on modification.
template<typename K, typename V>
using HashMap = boost::container::flat_map<K, V>;

/// A synchronized hash map is a hash map in which each
/// access is thread-safe. In this case, this is achieved by locking
/// each access with a mutex (which often ends up being a futex in the implementation).
template<typename K, typename V>
using SynchronizedHashMap = boost::synchronized_value<boost::container::flat_map<K, V>>;

