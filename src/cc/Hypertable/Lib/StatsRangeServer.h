/* -*- c++ -*-
 * Copyright (C) 2007-2015 Hypertable, Inc.
 *
 * This file is part of Hypertable.
 *
 * Hypertable is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 3 of the
 * License.
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

#ifndef Hypertable_Lib_StatsRangeServer_h
#define Hypertable_Lib_StatsRangeServer_h

#include "StatsTable.h"

#include <Common/Properties.h>
#include <Common/StatsSerializable.h>
#include <Common/StatsSystem.h>
#include <Common/StringExt.h>
#include <Common/SystemInfo.h>

#include <boost/algorithm/string.hpp>

#include <memory>
#include <vector>

namespace Hypertable {

  typedef std::map<const char*, StatsTable *, LtCstr> StatsTableMap;

  class StatsRangeServer : public StatsSerializable {
    
  public:

    StatsRangeServer();

    StatsRangeServer(PropertiesPtr &props);

    StatsRangeServer(const StatsRangeServer &other);

    void set_location( const std::string &loc ) {
      if (loc != location)
        location = loc;
    }
    void set_version( const std::string &ver ) {
      if (ver != version)
        version = ver;
    }
    bool operator==(const StatsRangeServer &other) const;
    bool operator!=(const StatsRangeServer &other) const {
      return !(*this == other);
    }

    std::string location;
    std::string version;
    int64_t timestamp {};
    int32_t range_count {};
    int32_t scanner_count {};
    uint64_t file_count {};
    uint64_t scan_count {};
    uint64_t scanned_cells {};
    uint64_t scanned_bytes {};
    uint64_t update_count {};
    uint64_t updated_cells {};
    uint64_t updated_bytes {};
    uint64_t sync_count {};
    uint64_t query_cache_max_memory {};
    uint64_t query_cache_available_memory {};
    uint64_t query_cache_accesses {};
    uint64_t query_cache_hits {};
    uint64_t block_cache_max_memory {};
    uint64_t block_cache_available_memory {};
    uint64_t block_cache_accesses {};
    uint64_t block_cache_hits {};
    uint64_t tracked_memory {};
    double   cpu_user {};
    double   cpu_sys {};
    bool     live {};

    StatsSystem system;
    std::vector<StatsTable> tables;
    StatsTableMap table_map;

  protected:
    virtual size_t encoded_length_group(int group) const;
    virtual void encode_group(int group, uint8_t **bufp) const;
    virtual void decode_group(int group, uint16_t len, const uint8_t **bufp, size_t *remainp);

  };

  /// Smart pointer to StatsRangeServer
  typedef std::shared_ptr<StatsRangeServer> StatsRangeServerPtr;

}

#endif // Hypertable_Lib_StatsRangeServer_h


