/* -*- c++ -*-
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

#ifndef Common_fast_clock_h
#define Common_fast_clock_h

#include <chrono>
#include <ctime>

namespace std {

  namespace chrono {

    class fast_clock {
    public:
      typedef microseconds duration;
      typedef duration::rep rep;
      typedef duration::period period;
      typedef chrono::time_point<fast_clock> time_point;
      static constexpr bool is_steady = false;

      static time_point now() noexcept;
      static time_t     to_time_t  (const time_point& __t) noexcept;
      static time_point from_time_t(time_t __t) noexcept;
    };

  }
}

#endif // Common_fast_clock_h
