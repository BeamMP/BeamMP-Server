#pragma once

#include <boost/thread/synchronized_value.hpp>
#include <mutex>

/// This header provides convenience aliases for synchronization primitives.

template <typename T>
using Sync = boost::synchronized_value<T, std::recursive_mutex>;
