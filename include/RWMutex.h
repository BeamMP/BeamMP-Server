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

/*
 * An RWMutex allows multiple simultaneous readlocks but only one writelock at a time,
 * and write locks and read locks are mutually exclusive.
 */

#include <shared_mutex>
#include <mutex>

// Use ReadLock(m) and WriteLock(m) to lock it.
using RWMutex = std::shared_mutex;
// Construct with an RWMutex as a non-const reference.
// locks the mutex in lock_shared mode (for reading). Locking in a thread that already owns a lock
// i.e. locking multiple times successively is UB. Construction may be blocking. Destruction is guaranteed to release the lock.
using ReadLock = std::shared_lock<RWMutex>;
// Construct with an RWMutex as a non-const reference.
// locks the mutex for writing. Construction may be blocking. Destruction is guaranteed to release the lock.
using WriteLock = std::unique_lock<RWMutex>;
