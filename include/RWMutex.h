// Author: lionkor
#pragma once

/*
 * An RWMutex allows multiple simultaneous readlocks but only one writelock at a time,
 * and write locks and read locks are mutually exclusive.
 */

#include <mutex>
#include <shared_mutex>

// Use ReadLock(m) and WriteLock(m) to lock it.
using RWMutex = std::shared_mutex;
// Construct with an RWMutex as a non-const reference.
// locks the mutex in lock_shared mode (for reading). Locking in a thread that already owns a lock
// i.e. locking multiple times successively is UB. Construction may be blocking. Destruction is guaranteed to release the lock.
using ReadLock = std::shared_lock<RWMutex>;
// Construct with an RWMutex as a non-const reference.
// locks the mutex for writing. Construction may be blocking. Destruction is guaranteed to release the lock.
using WriteLock = std::unique_lock<RWMutex>;
