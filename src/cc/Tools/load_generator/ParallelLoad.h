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

#ifndef Tools_load_generator_ParallelLoad_h
#define Tools_load_generator_ParallelLoad_h

#include <Hypertable/Lib/Cell.h>
#include <Hypertable/Lib/KeySpec.h>

#include <condition_variable>
#include <list>
#include <mutex>

namespace Hypertable {

  class LoadRec {
  public:
  LoadRec() : is_delete(false) { }

  LoadRec(Cell &other, bool is_del=false) : is_delete(is_del) {
      size_t data_len = strlen((const char *)other.row_key) +
      + strlen(other.column_family)
      + (other.column_qualifier ? strlen(other.column_qualifier) : 0)
      + (is_delete ? 0 : other.value_len) + 3;

      arena = new char [data_len];
      char *ptr = arena;

      // row
      strcpy(ptr, other.row_key);
      key.row_len = strlen(ptr);
      key.row = ptr;
      ptr += strlen(ptr) + 1;

      // column family
      strcpy(ptr, other.column_family);
      key.column_family = ptr;
      ptr += strlen(ptr) + 1;

      // column qualifier
      if (other.column_qualifier) {
        strcpy(ptr, other.column_qualifier);
        key.column_qualifier = ptr;
      }
      else
        key.column_qualifier = 0;
      ptr += strlen(ptr) + 1;

      // timestamp & revision
      key.timestamp = other.timestamp;
      key.revision = other.revision;

      if (!is_delete && other.value_len) {
        memcpy(ptr, other.value, other.value_len);
        value = ptr;
        value_len = other.value_len;
      }
      else
        value_len = 0;
    }

    ~LoadRec() {
      delete [] arena;
    }

    KeySpec key;
    bool is_delete;
    char *arena;
    const void *value;
    ::uint32_t value_len;
    unsigned long amount;
  };

  class ParallelStateRec {
  public:
    ParallelStateRec() { }

    ParallelStateRec(const ParallelStateRec& other) {
      total_cells = other.total_cells;
      total_bytes = other.total_bytes;
      finished = other.finished;
      cum_latency = other.cum_latency;
      cum_sq_latency = other.cum_sq_latency;
      min_latency = other.min_latency;
      max_latency = other.max_latency;
      elapsed_time = other.elapsed_time;
    }

    std::mutex mutex;
    std::condition_variable cond;
    std::list<LoadRec *> requests;
    std::list<LoadRec *> garbage;
    int64_t total_cells {};
    int64_t total_bytes {};
    bool finished {};
    double cum_latency {};
    double cum_sq_latency {};
    double min_latency {};
    double max_latency {};
    double elapsed_time {};
  };

}

#endif // Tools_load_generator_ParallelLoad_h
