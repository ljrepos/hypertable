/* -*- c++ -*-
 * Copyright (C) 2007-2015 Hypertable, Inc.
 *
 * This file is part of Hypertable.
 *
 * Hypertable is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or any later version.
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

/** @file
 * A timer class to keep timeout states across AsyncComm related calls.
 */

#ifndef Common_Timer_h
#define Common_Timer_h

#include <Common/fast_clock.h>

#include <cassert>
#include <chrono>
#include <cstring>

namespace Hypertable {

  /** @addtogroup Common
   *  @{
   */

  /**
   * A timer class to keep timeout states across AsyncComm related calls.
   */
  class Timer {
  public:
    /**
     * Constructor; assigns number of milliseconds after which the timer will
     * expire
     *
     * @param millis Number of milliseconds after timer will expire
     * @param start_timer If true, timer is started immediately; otherwise
     *      start with %start
     */
  Timer(uint32_t millis, bool start_timer = false)
    : m_duration(std::chrono::milliseconds(millis)),
      m_remaining(std::chrono::milliseconds(millis)) {
      if (start_timer)
        start();
    }

    /**
     * Starts the timer. Will have no effect if the timer is still running.
     */
    void start() {
      if (!m_running) {
        start_time = std::chrono::fast_clock::now();
        m_running = true;
        if (!m_started)
          m_started = true;
      }
    }

    /**
     * Stops the timer. Will assert that the timer was started. Updates the
     * remaining time (see %remaining).
     */
    void stop() {
      assert(m_started);
      auto adjustment = std::chrono::fast_clock::now() - start_time;
      m_remaining = (adjustment < m_remaining) ?
        m_remaining - adjustment :
        std::chrono::fast_clock::duration::zero();
      m_running = false;
    }

    /**
     * Resets the timer
     */
    void reset(bool start_timer = false) {
      m_running = false;
      m_started = false;
      m_remaining = m_duration;
      if (start_timer)
        start();
    }

    /**
     * Returns the remaining time till expiry
     * @return Remaining time in milliseconds
     */
    uint32_t remaining() {
      if (m_running) {
        stop();
        start();
      }
     return std::chrono::duration_cast<std::chrono::milliseconds>(m_remaining).count();
    }

    /**
     * Returns true if the timer is expired
     */
    bool expired() {
      return remaining() == 0;
    }

    /**
     * Returns true if the timer is still running (not yet expired
     */
    bool is_running() {
      return m_running;
    }

    /**
     * Returns the duration of the timer
     * @return Timer duration in milliseconds
     */
    uint32_t duration() {
      return (uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(m_duration).count();
    }


  private:

    /** The time when the timer was started */
    std::chrono::fast_clock::time_point start_time;

    /// True if the timer is running
    bool m_running {};

    /// True if the timer was started
    bool m_started {};

    /// The duration of the timer
    std::chrono::fast_clock::duration m_duration;

    /// The remaining time till expiration
    std::chrono::fast_clock::duration m_remaining;
  };

  /** @} */

}

#endif // Common_Timer_h
