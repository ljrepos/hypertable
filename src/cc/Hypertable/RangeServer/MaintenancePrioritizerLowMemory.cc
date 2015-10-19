/*
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
#include "Common/Compat.h"
#include "Common/Config.h"
#include "Common/ScopeGuard.h"
#include "Common/StringExt.h"

#include <iostream>

#include "Global.h"
#include "MaintenanceFlag.h"
#include "MaintenancePrioritizerLowMemory.h"

using namespace Hypertable;
using namespace Hypertable::Config;
using namespace std;

void
MaintenancePrioritizerLowMemory::prioritize(std::vector<RangeData> &range_data,
                                            MemoryState &memory_state,
                                            int32_t priority, String *trace) {
  LoadStatistics::Bundle load_stats;
  std::vector<RangeData> range_data_root;
  std::vector<RangeData> range_data_metadata;
  std::vector<RangeData> range_data_system;
  std::vector<RangeData> range_data_user;

  for (size_t i=0; i<range_data.size(); i++) {
    if (range_data[i].range->is_root())
      range_data_root.push_back(range_data[i]);
    else if (range_data[i].data->is_metadata)
      range_data_metadata.push_back(range_data[i]);
    else if (range_data[i].data->is_system)
      range_data_system.push_back(range_data[i]);
    else
      range_data_user.push_back(range_data[i]);
  }

  m_uninitialized_ranges_seen = false;

  /**
   * Assign priority for ROOT range
   */
  if (!range_data_root.empty()) {
    assign_priorities_all(range_data_root, Global::root_log,
                          Global::log_prune_threshold_min,
                          memory_state, priority, trace);
    schedule_initialization_operations(range_data_root, priority);
  }


  /**
   * Assign priority for METADATA ranges
   */
  if (!range_data_metadata.empty()) {
    assign_priorities_all(range_data_metadata, Global::metadata_log,
                          Global::log_prune_threshold_min, memory_state,
                          priority, trace);
    schedule_initialization_operations(range_data_metadata, priority);
  }

  /**
   *  Compute prune threshold based on load activity
   */
  Global::load_statistics->get(&load_stats);
  int64_t prune_threshold = (int64_t)(load_stats.update_mbps * (double)Global::log_prune_threshold_max);
  if (prune_threshold < Global::log_prune_threshold_min)
    prune_threshold = Global::log_prune_threshold_min;
  else if (prune_threshold > Global::log_prune_threshold_max)
    prune_threshold = Global::log_prune_threshold_max;
  if (trace)
    *trace += format("%d prune threshold\t%lld\n", __LINE__, (Lld)prune_threshold);

  /**
   * Assign priority for SYSTEM ranges
   */
  if (!range_data_system.empty()) {
    assign_priorities_all(range_data_system, Global::system_log, prune_threshold,
                          memory_state, priority, trace);
    schedule_initialization_operations(range_data_system, priority);
  }

  /**
   * Assign priority for USER ranges
   */
  if (!range_data_user.empty()) {

    if (schedule_inprogress_operations(range_data_user, memory_state, priority, trace))
      schedule_splits_and_relinquishes(range_data, memory_state, priority, trace);

    assign_priorities_user(range_data_user, load_stats, memory_state,
                           priority, trace);

    schedule_necessary_compactions(range_data_user, Global::user_log,
                                   prune_threshold, memory_state, priority,
                                   trace);

    schedule_initialization_operations(range_data_user, priority);
  }

  if (m_uninitialized_ranges_seen == false)
    m_initialization_complete = true;

}


/**
 * Memory freeing algorithm:
 *
 * 1. schedule in-progress relinquish and/or split operations
 * 2. schedule needed splits
 * 3. schedule needed compactions
 */
void
MaintenancePrioritizerLowMemory::assign_priorities_all(std::vector<RangeData> &range_data,
            CommitLogPtr &log, int64_t prune_threshold, MemoryState &memory_state,
	    int32_t &priority, String *trace) {

  if (!schedule_inprogress_operations(range_data, memory_state, priority, trace))
    return;

  if (!schedule_splits_and_relinquishes(range_data, memory_state, priority, trace))
    return;

  if (!schedule_necessary_compactions(range_data, log, prune_threshold,
                                      memory_state, priority, trace))
    return;

}


/**
 * Memory freeing algorithm:
 *
 * 1. purge shadow caches
 *
 * if (READ heavy)
 *   2. compact remaining cell caches
 *   3. shrink block cache
 *   4. purge cell store indexes
 * else if (WRITE heavy)
 *   2. shrink block cache
 *   3. purge cell store indexes
 *   4. compact remaining cell caches
 * else
 *   2. shrink block cache
 *   3. compact remaining cell caches
 *   4. purge cell store indexes
 *
 */

void MaintenancePrioritizerLowMemory::assign_priorities_user(
       std::vector<RangeData> &range_data, LoadStatistics::Bundle &load_stats,
       MemoryState &memory_state, int32_t &priority, String *trace) {

  if (!purge_shadow_caches(range_data, memory_state, priority, trace))
    return;

  if (load_stats.update_bytes < 500000 && load_stats.scan_count > 10) {

    HT_INFOF("READ workload prioritization (update_bytes=%llu, scan_count=%u)",
	     (Llu)load_stats.update_bytes, (unsigned)load_stats.scan_count);

    if (!compact_cellcaches(range_data, memory_state, priority, trace))
      return;

    if (Global::block_cache) {
      Global::block_cache->cap_memory_use();
      memory_state.decrement_needed( Global::block_cache->decrease_limit(memory_state.needed) );
      if (!memory_state.need_more())
        return;
    }

    if (!purge_cellstore_indexes(range_data, memory_state, priority, trace))
      return;

  }
  else {

    HT_INFOF("WRITE workload prioritization (update_bytes=%llu, scan_count=%u)",
	     (Llu)load_stats.update_bytes, (unsigned)load_stats.scan_count);

    if (Global::block_cache) {
      Global::block_cache->cap_memory_use();
      memory_state.decrement_needed( Global::block_cache->decrease_limit(memory_state.needed) );
      if (!memory_state.need_more())
        return;
    }

    if (!purge_cellstore_indexes(range_data, memory_state, priority, trace))
      return;

    if (!compact_cellcaches(range_data, memory_state, priority, trace))
      return;

  }

}
