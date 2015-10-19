/*
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
 * Declarations for ExpireTimer.
 * This file contains type declarations for ExpireTimer, a structure for
 * holding timer state.
 */

#ifndef AsyncComm_ExpireTimer_h
#define AsyncComm_ExpireTimer_h

#include "Clock.h"
#include "DispatchHandler.h"

namespace Hypertable {

  /** @addtogroup AsyncComm
   *  @{
   */

  /** State record for timer.
   */
  struct ExpireTimer {
    ClockT::time_point expire_time;   //!< Absolute expiration time
    DispatchHandlerPtr handler; //!< Dispatch handler to receive TIMER event
  };

  /** Comparison function (functor) for timer heap.
   */
  struct LtTimerHeap {
    /** Parenthesis operator with two ExpireTimer parameters.
     * Provides "largest first" comparison.
     * @param t1 Lefthand side of comparison
     * @param t2 Righthand side of comparison
     * @return true if <code>t1</code> is greater than <code>t2</code>
     */
    bool operator()(const ExpireTimer &t1, const ExpireTimer &t2) const {
      return std::chrono::operator>(t1.expire_time, t2.expire_time);
    }
  };
  /** @}*/
}

#endif // AsyncComm_ExpireTimer_h
