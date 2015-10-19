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

#include "TableCache.h"

using namespace Hypertable;
using namespace std;

TableCache::TableCache(PropertiesPtr &props, RangeLocatorPtr &range_locator,
    ConnectionManagerPtr &conn_manager, Hyperspace::SessionPtr &hyperspace,
    ApplicationQueueInterfacePtr &app_queue, NameIdMapperPtr &namemap, 
    uint32_t default_timeout_ms)
  : m_props(props), m_range_locator(range_locator), 
    m_comm(conn_manager->get_comm()), m_conn_manager(conn_manager), 
    m_hyperspace(hyperspace), m_app_queue(app_queue),
    m_namemap(namemap), m_timeout_ms(default_timeout_ms) {
  HT_ASSERT(m_props && m_range_locator && conn_manager && m_hyperspace &&
            m_app_queue && m_namemap);
  m_hyperspace->add_callback(this);
}

TableCache::~TableCache() {
  m_hyperspace->remove_callback(this);
}

TablePtr TableCache::get(const string &table_name, int32_t flags) {
  lock_guard<mutex> lock(m_mutex);
  return get_unlocked(table_name, flags);
}

TablePtr TableCache::get_unlocked(const string &table_name, int32_t flags) {
  string id;

  TableMap::iterator it = m_table_map.find(table_name);
  if (it != m_table_map.end()) {
    if ((flags & Table::OPEN_FLAG_REFRESH_TABLE_CACHE) || it->second->need_refresh())
      it->second->refresh();
    return it->second;
  }

  TablePtr table = 
    make_shared<Table>(m_props, m_range_locator, m_conn_manager, 
                       m_hyperspace, m_app_queue, m_namemap, table_name, 
                       flags, m_timeout_ms);

  m_table_map.insert(make_pair(table_name, table));

  return table;
}

bool TableCache::get_schema_str(const string &table_name, string &schema, bool with_ids)
{
  lock_guard<mutex> lock(m_mutex);
  TableMap::const_iterator it = m_table_map.find(table_name);

  if (it == m_table_map.end())
    return false;
  schema = it->second->schema()->render_xml(with_ids);
  return true;
}

bool TableCache::get_schema(const string &table_name, SchemaPtr &output_schema) {
  lock_guard<mutex> lock(m_mutex);
  TableMap::const_iterator it = m_table_map.find(table_name);

  if (it == m_table_map.end())
    return false;
  output_schema = make_shared<Schema>(*(it->second->schema()));
  return true;
}

bool TableCache::remove(const string &table_name) {
  lock_guard<mutex> lock(m_mutex);
  bool found = false;
  TableMap::iterator it = m_table_map.find(table_name);

  if (it != m_table_map.end()) {
    found = true;
    m_table_map.erase(it);
  }
  return found;
}

void TableCache::reconnected() {
  lock_guard<mutex> lock(m_mutex);
  for( TableMap::iterator it = m_table_map.begin(); it != m_table_map.end(); ++it)
    (*it).second->invalidate();
}
