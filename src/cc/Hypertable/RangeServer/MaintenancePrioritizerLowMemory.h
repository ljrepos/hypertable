/* -*- c++ -*-
 * Copyright (C) 2007-2015 Hypertable, Inc.
 *
 * This file is part of Hypertable.
 *
 * Hypertable is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 3 of the
 * License, or any later version.
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

#ifndef HYPERTABLE_MAINTENANCEPRIORITIZERLOWMEMORY_H
#define HYPERTABLE_MAINTENANCEPRIORITIZERLOWMEMORY_H

#include "MaintenancePrioritizer.h"

namespace Hypertable {

  class MaintenancePrioritizerLowMemory : public MaintenancePrioritizer {
  public:

    virtual void prioritize(std::vector<RangeData> &range_data, MemoryState &memory_state,
                            int32_t priority, String *trace);

  private:

    void assign_priorities_all(std::vector<RangeData> &range_data, CommitLogPtr &log,
                               int64_t prune_threshold, MemoryState &memory_state,
                               int32_t &priority, String *trace);

    void assign_priorities_user(std::vector<RangeData> &range_data,
                                LoadStatistics::Bundle &load_stats,
                                MemoryState &memory_state,
                                int32_t &priority, String *trace);

  };

}

#endif // HYPERTABLE_MAINTENANCEPRIORITIZERLOWMEMORY_H


