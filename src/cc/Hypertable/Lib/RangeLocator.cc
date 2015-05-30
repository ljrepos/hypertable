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

#include <Common/Compat.h>

#include "Hyperspace/Session.h"

#include <Hypertable/Lib/Key.h>
#include <Hypertable/Lib/NameIdMapper.h>
#include <Hypertable/Lib/RootFileHandler.h>
#include <Hypertable/Lib/RangeLocator.h>
#include <Hypertable/Lib/ScanBlock.h>
#include <Hypertable/Lib/ScanSpec.h>

#include <Common/Error.h>
#include <Common/ScopeGuard.h>

#include <boost/algorithm/string.hpp>

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <vector>

extern "C" {
#include <limits.h>
#include <poll.h>
#include <string.h>
}

// convenient local macros to record errors for debugging
#define SAVE_ERR(_code_, _msg_) \
  do { \
    ScopedLock lock(m_mutex); \
    m_last_errors.push_back(HT_EXCEPTION(_code_, _msg_)); \
    while (m_last_errors.size() > m_max_error_queue_length) \
      m_last_errors.pop_front(); \
  } while (false)

#define SAVE_ERR2(_code_, _ex_, _msg_) \
  do { \
    ScopedLock lock(m_mutex); \
    m_last_errors.push_back(HT_EXCEPTION2(_code_, _ex_, _msg_)); \
    while (m_last_errors.size() > m_max_error_queue_length) \
      m_last_errors.pop_front(); \
  } while (false)

using namespace Hypertable;
using namespace Hypertable::Lib;
using namespace std;

namespace {

  class MetaKeyBuilder {
  public:
    MetaKeyBuilder() : start(buf_start), end(buf_end) { }
    void
    build_keys(const char *format, const char *table_name, const char *row_key) {
      int len_end = strlen(format) + strlen(table_name) + 3;
      int len_start = len_end;
      if (row_key) {
        len_start += strlen(row_key);
        if( len_start > size ) start = new char [len_start];
        sprintf(start, format, table_name);
        strcat(start, row_key);
      }
      else {
        if( len_start > size ) start = new char [len_start];
        sprintf(start, format, table_name);
      }
      if( len_end > size ) end = new char [len_end];
      sprintf(end, format, table_name);
      char *ptr = end + strlen(end);
      *ptr++ = (char)0xff;
      *ptr++ = (char)0xff;
      *ptr = 0;
    }
    ~MetaKeyBuilder() {
      if (start != buf_start) delete [] start;
      if (end != buf_end) delete [] end;
    }
    char *start;
    char *end;

  private:
    enum { size = 64 };
    char buf_start[size];
    char buf_end[size];
  };
}


RangeLocator::RangeLocator(PropertiesPtr &cfg, ConnectionManagerPtr &conn_mgr,
    Hyperspace::SessionPtr &hyperspace, uint32_t timeout_ms)
  : m_conn_manager(conn_mgr), m_hyperspace(hyperspace),
    m_root_stale(true), m_range_server(conn_mgr->get_comm(), timeout_ms),
    m_hyperspace_init(false), m_hyperspace_connected(true),
    m_timeout_ms(timeout_ms) {

  m_metadata_readahead_count
      = cfg->get_i32("Hypertable.RangeLocator.MetadataReadaheadCount");
  m_max_error_queue_length
      = cfg->get_i32("Hypertable.RangeLocator.MaxErrorQueueLength");
  m_metadata_retry_interval
      = cfg->get_i32("Hypertable.RangeLocator.MetadataRetryInterval");
  m_root_metadata_retry_interval
      = cfg->get_i32("Hypertable.RangeLocator.RootMetadataRetryInterval");

  int cache_size = cfg->get_i64("Hypertable.LocationCache.MaxEntries");

  m_toplevel_dir = cfg->get_str("Hypertable.Directory");
  boost::trim_if(m_toplevel_dir, boost::is_any_of("/"));
  m_toplevel_dir = String("/") + m_toplevel_dir;

  m_cache = new LocationCache(cache_size);
  // register hyperspace session callback
  m_hyperspace_session_callback.m_rangelocator = this;
  m_hyperspace->add_callback(&m_hyperspace_session_callback);
  // no need to serialize access in ctor
  initialize();
}

void RangeLocator::hyperspace_disconnected()
{
  ScopedLock lock(m_hyperspace_mutex);
  m_hyperspace_init = false;
  m_hyperspace_connected = false;
}

void RangeLocator::hyperspace_reconnected()
{
  ScopedLock lock(m_hyperspace_mutex);
  HT_ASSERT(!m_hyperspace_init);
  m_hyperspace_connected = true;
}

/**
 * Assumes access is serialized via m_hyperspace_mutex
 */
void RangeLocator::initialize() {
  DynamicBuffer valbuf(0);
  uint64_t handle = 0;
  Timer timer(m_timeout_ms, true);

  if (m_hyperspace_init)
    return;
  HT_ASSERT(m_hyperspace_connected);

  m_root_handler = new RootFileHandler(this);

  m_root_file_handle = m_hyperspace->open(m_toplevel_dir + "/root",
                                          OPEN_FLAG_READ, m_root_handler);

  while (true) {
    string metadata_file = m_toplevel_dir + "/tables/" + TableIdentifier::METADATA_ID;

    try {
      handle = m_hyperspace->open(metadata_file, OPEN_FLAG_READ);
      break;
    }
    catch (Exception &e) {
      if (timer.expired())
        HT_THROW2(Error::HYPERSPACE_FILE_NOT_FOUND, e, metadata_file);
      poll(0, 0, 3000);
    }
  }

  HT_ON_SCOPE_EXIT(&Hyperspace::close_handle, m_hyperspace, handle);

  while (true) {
    try {
      m_hyperspace->attr_get(handle, "schema", valbuf);
      break;
    }
    catch (Exception &) {
      if (timer.expired()) {
        m_hyperspace->close_nowait(handle);
        throw;
      }
      poll(0, 0, 3000);
    }
  }

  SchemaPtr schema = Schema::new_instance((const char *)valbuf.base);

  m_metadata_table.id = TableIdentifier::METADATA_ID;
  m_metadata_table.generation = schema->get_generation();

  ColumnFamilySpec *cf_spec;

  if ((cf_spec = schema->get_column_family("StartRow")) == 0) {
    HT_ERROR("Unable to find column family 'StartRow' in METADATA schema");
    HT_THROW_(Error::BAD_SCHEMA);
  }
  m_startrow_cid = cf_spec->get_id();

  if ((cf_spec = schema->get_column_family("Location")) == 0) {
    HT_ERROR("Unable to find column family 'Location' in METADATA schema");
    HT_THROW_(Error::BAD_SCHEMA);
  }
  m_location_cid = cf_spec->get_id();
  m_hyperspace_init = true;
}


RangeLocator::~RangeLocator() {
  m_hyperspace->close_nowait(m_root_file_handle);
  m_hyperspace->remove_callback(&m_hyperspace_session_callback);
}


void
RangeLocator::find_loop(const TableIdentifier *table, const char *row_key,
    RangeLocationInfo *rane_loc_infop, Timer &timer, bool hard) {
  int error;
  uint32_t wait_time = 1000;
  uint32_t total_wait_time = 0;

  error = find(table, row_key, rane_loc_infop, timer, hard);

  if (error == Error::TABLE_NOT_FOUND) {
    clear_error_history();
    HT_THROWF(error, "Table '%s' is (being) dropped", table->id);
  }

  while (error != Error::OK) {

    // check for timer expiration
    if (timer.remaining() < wait_time) {
      dump_error_history();
      HT_THROWF(Error::REQUEST_TIMEOUT, "Locating range for table %s row='%s'", table->id, row_key);
    }

    // wait a bit
    poll(0, 0, (int)wait_time);
    total_wait_time += wait_time;
    wait_time = (wait_time * 3) / 2;

    // try again
    if ((error = find(table, row_key, rane_loc_infop, timer, true))
        == Error::TABLE_NOT_FOUND) {
      clear_error_history();
      HT_THROWF(error, "Table '%s' is (being) dropped", table->id);
    }
  }

  clear_error_history();
}


int
RangeLocator::find(const TableIdentifier *table, const char *row_key,
    RangeLocationInfo *rane_loc_infop, Timer &timer, bool hard) {
  RangeSpec range;
  ScanSpec meta_scan_spec;
  vector<ScanBlock> scan_blocks(1);
  int error;
  Key key;
  RangeLocationInfo range_loc_info;
  string start_row;
  string end_row;
  CommAddress addr;
  RowInterval ri;
  bool inclusive = (row_key == 0 || *row_key == 0) ? true : false;
  bool root_lookup = table->is_metadata() && (row_key == 0 || strcmp(row_key, Key::END_ROOT_ROW) <= 0);

  //HT_DEBUG_OUT << "Trying to locate " << table->id <<  "[" << row_key
  //    << "]" << " hard=" << hard << " m_root_stale=" << m_root_stale << " root_addr="
  //    << m_root_range_info.addr.to_str() << HT_END;

  if (m_root_stale || (hard && root_lookup)) {
    if ((error = read_root_location(timer)) != Error::OK)
      return error;
  }

  {
    ScopedLock lock(m_mutex);
    addr = m_root_range_info.addr;
  }

  if (!hard && m_cache->lookup(table->id, row_key, rane_loc_infop))
    return Error::OK;

  /**
   * If key is on ROOT metadata range, return root range information
   */
  if (root_lookup) {
    rane_loc_infop->start_row = "";
    rane_loc_infop->end_row = Key::END_ROOT_ROW;
    rane_loc_infop->addr = addr;
    return Error::OK;
  }

  /** at this point, we didn't find it so we need to do a METADATA lookup **/

  range.start_row = 0;
  range.end_row = Key::END_ROOT_ROW;

  MetaKeyBuilder meta_keys;
  char *meta_key;

  if (table->is_metadata())
    meta_keys.build_keys("%s:", TableIdentifier::METADATA_ID, row_key);
  else {
    char format_str[8];
    sprintf(format_str, "%s:%%s:", TableIdentifier::METADATA_ID);
    meta_keys.build_keys(format_str, table->id, row_key);
  }

  /**
   * Find second level METADATA range from root
   */
  meta_key = meta_keys.start + TableIdentifier::METADATA_ID_LENGTH + 1;
  if (hard || !m_cache->lookup(TableIdentifier::METADATA_ID, meta_key,
                               rane_loc_infop, inclusive)) {

    meta_scan_spec.row_limit = m_metadata_readahead_count;
    meta_scan_spec.max_versions = 1;
    meta_scan_spec.columns.push_back("StartRow");
    meta_scan_spec.columns.push_back("Location");

    ri.start = meta_keys.start;
    ri.start_inclusive = true;
    ri.end = 0;
    ri.end_inclusive = false;
    meta_scan_spec.row_intervals.push_back(ri);

    meta_scan_spec.return_deletes = false;
    // meta_scan_spec.interval = ????;

    try {
      //HT_DEBUG_OUT << "Trying to locate " << table->id <<  "[" << row_key
      //    << "]" << " hard=" << hard << " m_root_stale=" << m_root_stale << " root_addr="
      //    << m_root_range_info.addr.to_str() << " scanning metadata at addr "
      //    << addr.to_str() << " scan_spec=" << meta_scan_spec << HT_END;

      m_range_server.create_scanner(addr, m_metadata_table, range,
                                    meta_scan_spec, scan_blocks.back(), timer);
      while (!scan_blocks.back().eos()) {
        int scanner_id = scan_blocks.back().get_scanner_id();
        scan_blocks.resize(scan_blocks.size()+1);
        m_range_server.fetch_scanblock(addr, scanner_id, scan_blocks.back());
      }
    }
    catch (Exception &e) {
      if (e.code() == Error::COMM_NOT_CONNECTED ||
          e.code() == Error::COMM_BROKEN_CONNECTION ||
          e.code() == Error::COMM_INVALID_PROXY)
        invalidate_host(addr.proxy);

      m_root_stale = true;

      SAVE_ERR2(e.code(), e, format("Problem creating scanner for start row "
                "'%s' on METADATA[..??]", meta_keys.start));
      return e.code();
    }
    catch (std::exception &e) {
      HT_INFOF("std::exception - %s", e.what());
      SAVE_ERR(Error::COMM_SEND_ERROR, e.what());
      return Error::COMM_SEND_ERROR;
    }

    if ((error = process_metadata_scanblocks(scan_blocks, timer)) != Error::OK) {
      m_root_stale = true;
      return error;
    }

    if (!m_cache->lookup(TableIdentifier::METADATA_ID, meta_key,
                         rane_loc_infop, inclusive)) {
      string err_msg = format("Unable to find metadata for row '%s' row_key=%s",
                              meta_keys.start, row_key);
      HT_INFOF("%s", err_msg.c_str());
      SAVE_ERR(Error::METADATA_NOT_FOUND, err_msg);
      return Error::METADATA_NOT_FOUND;
    }
  }

  if (table->is_metadata())
    return Error::OK;

  /**
   * Find actual range from second-level METADATA range
   */

  range.start_row = rane_loc_infop->start_row.c_str();
  range.end_row   = rane_loc_infop->end_row.c_str();

  addr = rane_loc_infop->addr;

  meta_scan_spec.clear();

  meta_scan_spec.row_limit = m_metadata_readahead_count;
  meta_scan_spec.max_versions = 1;
  meta_scan_spec.columns.push_back("StartRow");
  meta_scan_spec.columns.push_back("Location");

  ri.start = meta_keys.start+TableIdentifier::METADATA_ID_LENGTH + 1;
  ri.start_inclusive = true;
  ri.end = meta_keys.end+TableIdentifier::METADATA_ID_LENGTH+1;;
  ri.end_inclusive = true;
  meta_scan_spec.row_intervals.push_back(ri);

  // meta_scan_spec.interval = ????;

  try {
    scan_blocks.clear();
    scan_blocks.resize(1);
    m_range_server.create_scanner(addr, m_metadata_table, range,
                                  meta_scan_spec, scan_blocks.back(), timer);

    while (!scan_blocks.back().eos()) {
      int scanner_id = scan_blocks.back().get_scanner_id();
      scan_blocks.resize(scan_blocks.size()+1);
      m_range_server.fetch_scanblock(addr, scanner_id, scan_blocks.back());
    }
  }
  catch (Exception &e) {
    if (e.code() == Error::COMM_NOT_CONNECTED ||
        e.code() == Error::COMM_BROKEN_CONNECTION ||
        e.code() == Error::COMM_INVALID_PROXY)
      invalidate_host(addr.proxy);
    else if (e.code() == Error::RANGESERVER_RANGE_NOT_FOUND)
      m_cache->invalidate(TableIdentifier::METADATA_ID,
                          meta_keys.start+TableIdentifier::METADATA_ID_LENGTH+1);
    SAVE_ERR2(e.code(), e, format("Problem creating scanner on second-level "
              "METADATA (start row = %s)", ri.start));
    return e.code();
  }
  catch (std::exception &e) {
    HT_INFOF("std::exception - %s", e.what());
    SAVE_ERR(Error::COMM_SEND_ERROR, e.what());
    return Error::COMM_SEND_ERROR;
  }

  if ((error = process_metadata_scanblocks(scan_blocks, timer)) != Error::OK)
    return error;

  if (row_key == 0)
    row_key = "";

  if (!m_cache->lookup(table->id, row_key, rane_loc_infop, inclusive)) {
    SAVE_ERR(Error::METADATA_NOT_FOUND, (String)"RangeLocator failed to find "
             "metadata for table '" + table->id + "' row '" + row_key + "'");
    return Error::METADATA_NOT_FOUND;
  }

  return Error::OK;
}


int RangeLocator::process_metadata_scanblocks(vector<ScanBlock> &scan_blocks, Timer &timer) {
  RangeLocationInfo range_loc_info;
  SerializedKey serkey;
  ByteString value;
  Key key;
  const char *stripped_key;
  string table_name;
  CommAddressSet connected;

  range_loc_info.start_row = "";
  range_loc_info.end_row = "";
  range_loc_info.addr.clear();

  bool got_start_row = false;
  bool got_end_row = false;
  bool got_location = false;

  for (auto & scan_block : scan_blocks) {

    while (scan_block.next(serkey, value)) {

      if (!key.load(serkey)) {
        string err_msg = format("METADATA lookup for '%s' returned bad key",
                                serkey.str() + 1);
        HT_ERRORF("%s", err_msg.c_str());
        SAVE_ERR(Error::INVALID_METADATA, err_msg);
        return Error::INVALID_METADATA;
      }

      if ((stripped_key = strchr(key.row, ':')) == 0) {
        string err_msg = format("Bad row key found in METADATA - '%s'", key.row);
        HT_ERRORF("%s", err_msg.c_str());
        SAVE_ERR(Error::INVALID_METADATA, err_msg);
        return Error::INVALID_METADATA;
      }
      stripped_key++;
#if 0
      {
        const uint8_t *str;
        size_t len = value.decode_length(&str);
        string tmp_str = String((const char *)str, len);
        HT_DEBUG_OUT << "Got key=" << key << ", stripped_key "
                     << stripped_key << ", value=" << tmp_str << " got start_row="
                     << got_start_row << ", got_end_row=" << got_end_row
                     << ", got_location=" << got_location << HT_END;
      }
#endif
      if (got_end_row) {
        if (strcmp(stripped_key, range_loc_info.end_row.c_str())) {
          if (got_start_row && got_location) {

            // If not already connected, connect...
            if (connected.count(range_loc_info.addr) == 0) {
              if (connect(range_loc_info.addr, timer) == Error::OK)
                connected.insert(range_loc_info.addr);
            }

            m_cache->insert(table_name.c_str(), range_loc_info);

            //HT_DEBUG_OUT << "(1) cache insert table=" << table_name << " start="
            //    << range_loc_info.start_row << " end=" << range_loc_info.end_row
            //    << " loc=" << range_loc_info.addr.to_str() << HT_END;

          }
          else {
            //HT_DEBUG_OUT << "Incomplete METADATA record found =" << table_name << " end="
            //    << range_loc_info.end_row << " got start_row=" << got_start_row
            //    << ", got_end_row=" << got_end_row << ", got_location=" << got_location << HT_END;

            SAVE_ERR(Error::INVALID_METADATA, format("Incomplete METADATA record "
                                                     "found under row key '%s' (got_location=%s)", range_loc_info
                                                     .end_row.c_str(), got_location ? "true" : "false"));
          }
          range_loc_info.start_row = "";
          range_loc_info.end_row = "";
          range_loc_info.addr.clear();
          got_start_row = false;
          got_end_row = false;
          got_location = false;
        }
      }
      else {
        const char *colon = strchr(key.row, ':');
        assert(colon);
        table_name.clear();
        table_name.append(key.row, colon-key.row);
        range_loc_info.end_row = stripped_key;
        got_end_row = true;
      }

      if (key.column_family_code == m_startrow_cid) {
        const uint8_t *str;
        size_t len = value.decode_length(&str);
        //cout << "TS=" << key.timestamp << endl;
        range_loc_info.start_row = String((const char *)str, len);
        got_start_row = true;
      }
      else if (key.column_family_code == m_location_cid) {
        const uint8_t *str;
        size_t len = value.decode_length(&str);
        if (str[0] == '!' && len == 1)
          return Error::TABLE_NOT_FOUND;
        range_loc_info.addr.set_proxy( String((const char *)str, len));
        got_location = true;
      }
      else {
        HT_ERRORF("METADATA lookup on row '%s' returned incorrect column (id=%d)",
                  serkey.row(), key.column_family_code);
      }
    }

  }

  if (got_start_row && got_end_row && got_location) {

    // If not already connected, connect...
    if (connected.count(range_loc_info.addr) == 0) {
      if (connect(range_loc_info.addr, timer) == Error::OK)
        connected.insert(range_loc_info.addr);
    }

    m_cache->insert(table_name.c_str(), range_loc_info);

    //HT_DEBUG_OUT << "(2) cache insert table=" << table_name << " start="
    //    << range_loc_info.start_row << " end=" << range_loc_info.end_row
    //    << " loc=" << range_loc_info.addr.to_str() << HT_END;

  }
  else if (got_end_row) {
    //HT_DEBUG_OUT << "Incomplete METADATA record found =" << table_name << " end="
    //    << range_loc_info.end_row << " got start_row=" << got_start_row
    //    << ", got_end_row=" << got_end_row << ", got_location=" << got_location << HT_END;

    SAVE_ERR(Error::INVALID_METADATA, format("Incomplete METADATA record found "
             "under row key '%s' (got_location=%s)", range_loc_info
             .end_row.c_str(), got_location ? "true" : "false"));
  }

  return Error::OK;
}


int RangeLocator::read_root_location(Timer &timer) {
  DynamicBuffer value(0);
  string addr_str;
  CommAddress addr;
  CommAddress old_addr;

  {
    ScopedLock lock(m_hyperspace_mutex);
    if (m_hyperspace_init)
      m_hyperspace->attr_get(m_root_file_handle, "Location", value);
    else if (m_hyperspace_connected) {
      initialize();
      m_hyperspace->attr_get(m_root_file_handle, "Location", value);
      }
    else
      HT_THROW(Error::CONNECT_ERROR_HYPERSPACE, "RangeLocator not connected to Hyperspace");
  }

  {
    ScopedLock lock(m_mutex);
    old_addr = m_root_range_info.addr;
    m_root_range_info.start_row  = "";
    m_root_range_info.end_row    = Key::END_ROOT_ROW;
    m_root_range_info.addr.set_proxy( (const char *)value.base );
    m_cache->insert(TableIdentifier::METADATA_ID, m_root_range_info, true);
    addr = m_root_range_info.addr;
  }

  if (m_conn_manager) {

    /**
     * NOTE: This block assumes that a change of root address
     * means the old server has died.  If root changes in the
     * future can happen even when the original root server is
     * still alive, this will have to change.
     */
    if (old_addr.is_set() && old_addr != addr) {
      m_conn_manager->remove(old_addr);
      invalidate_host(old_addr.proxy);
    }

    m_conn_manager->add(addr, m_root_metadata_retry_interval,
                        "Root RangeServer");

    if (!m_conn_manager->wait_for_connection(addr, 10000)) {
      if (timer.expired()) {
        HT_ERRORF("Timeout waiting for root RangeServer connection - %s",
                  addr.to_str().c_str());
        return Error::REQUEST_TIMEOUT;
      }
      return Error::COMM_NOT_CONNECTED;
    }
  }

  m_root_stale = false;

  return Error::OK;
}

int RangeLocator::connect(CommAddress &addr, Timer &timer) {

  if (m_conn_manager) {
    m_conn_manager->add(addr, m_metadata_retry_interval, "RangeServer");
    if (!m_conn_manager->wait_for_connection(addr, 5000)) {
      if (timer.expired())
        return Error::REQUEST_TIMEOUT;
      return Error::COMM_NOT_CONNECTED;
    }
  }
  return Error::OK;
}

void RangeLocatorHyperspaceSessionCallback::disconnected() {
  m_rangelocator->hyperspace_disconnected();
}

void RangeLocatorHyperspaceSessionCallback::reconnected() {
  m_rangelocator->hyperspace_reconnected();
}


