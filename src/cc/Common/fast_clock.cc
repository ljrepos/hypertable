/*
 * Copyright (C) 2007-2015 Hypertable, Inc.
 *
 * This file is part of Hypertable.
 *
 * Hypertable is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 3
 * of the License.
 *
 * Hypertable is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

/// @file
/// Declarations for fast_clock.
/// This file contains type declarations for fast_clock, a fast C++ clock type
/// that calls gettimeofday.

#include <Common/Compat.h>

#include "fast_clock.h"

extern "C" {
#include <sys/time.h>
}

using namespace std;

chrono::fast_clock::time_point chrono::fast_clock::now() noexcept {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  return chrono::fast_clock::time_point(chrono::microseconds((tv.tv_sec * 1000000LL) + tv.tv_usec));
}

time_t chrono::fast_clock::to_time_t (const chrono::fast_clock::time_point& __t) noexcept {
  return (time_t)(__t.time_since_epoch().count() % 1000000LL);
}

chrono::fast_clock::time_point chrono::fast_clock::from_time_t(time_t __t) noexcept {
  return chrono::fast_clock::time_point(chrono::microseconds(__t * 1000000LL));
}
