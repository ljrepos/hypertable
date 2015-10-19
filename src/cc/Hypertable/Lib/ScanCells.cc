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

#include <Hypertable/Lib/ScanCells.h>

using namespace Hypertable;
using namespace Hypertable::Lib;
using namespace std;

bool ScanCells::add(EventPtr &event, int *scanner_id) {
  ScanBlockPtr scanblock = std::make_shared<ScanBlock>();
  scanblock->load(event);

  /// Aggregate profile data
  m_profile_data += scanblock->profile_data();
  m_profile_data.scanblocks++;

  m_scanblocks.push_back(scanblock);
  *scanner_id = scanblock->get_scanner_id();
  return scanblock->eos();
}

bool
ScanCells::load(SchemaPtr &schema, const string &end_row, bool end_inclusive,
		ScanLimitState *limit_state, CstrSet &rowset,
		int64_t *bytes_scanned, Key *lastkey) {
  SerializedKey serkey;
  ByteString value;
  Key key;
  Cell cell;
  ScanBlock *scanblock;
  ColumnFamilySpec *cf_spec;
  size_t total_cells=0;
  bool skipping = lastkey->row != 0;

  for(size_t ii=0; ii < m_scanblocks.size(); ++ii)
    total_cells += m_scanblocks[ii]->size();

  m_cells = make_shared<CellsBuilder>(total_cells);

  for (size_t ii=0; ii < m_scanblocks.size(); ++ii) {
    scanblock = m_scanblocks[ii].get();
    while (scanblock->next(serkey, value)) {

      if (skipping) {
        if (serkey <= lastkey->serial)
          continue;
        skipping = false;
      }

      if (!key.load(serkey))
        HT_THROW(Error::BAD_KEY, "");

      // check for end row
      if (!strcmp(key.row, Key::END_ROW_MARKER)) {
        return true;
      }

      if (end_inclusive) {
        if (strcmp(key.row, end_row.c_str()) > 0) {
          return true;
        }
      }
      else {
        if (strcmp(key.row, end_row.c_str()) >= 0) {
          return true;
        }
      }

      // check for row change and row limit
      if (limit_state->row_limit > 0) {
        if (strcmp(limit_state->last_row.c_str(), key.row)) {
          if (!limit_state->last_row.empty() &&
              limit_state->rows_seen < limit_state->rows_encountered) {
            limit_state->rows_seen++;
            if (limit_state->row_limit > 0 &&
                limit_state->rows_seen >= limit_state->row_limit)
              return true;
          }
          limit_state->rows_encountered++;
          limit_state->last_row = key.row;
        }
      }

      cell.row_key = key.row;
      cell.column_qualifier = key.column_qualifier;
      if ((cf_spec = schema->get_column_family(key.column_family_code)) == 0) {
        if (key.flag != FLAG_DELETE_ROW)
          HT_THROWF(Error::BAD_KEY, "Unexpected column family code %d",
              (int)key.column_family_code);
        cell.column_family = "";
      }
      else
        cell.column_family = cf_spec->get_name().c_str();

      cell.timestamp = key.timestamp;
      cell.revision = key.revision;
      cell.value_len = value.decode_length(&cell.value);
      cell.flag = key.flag;
      m_cells->add(cell, false);
      *bytes_scanned += key.length + cell.value_len;

      // if rowset scan remove scanned row
      while (!rowset.empty() && strcmp(*rowset.begin(), key.row) < 0)
        rowset.erase(rowset.begin());

      // Check for cell limit
      limit_state->cells_seen++;
      if (limit_state->cell_limit > 0 &&
          limit_state->cells_seen >= limit_state->cell_limit)
        return true;
    }

    // If at end of scan make sure last row encountered is reflected in rows seen
    if (scanblock->eos() &&
        limit_state->rows_encountered > limit_state->rows_seen) {
      limit_state->rows_seen++;
      HT_ASSERT(limit_state->rows_encountered == limit_state->rows_seen);
    }

    // Check row limit
    if (limit_state->row_limit > 0 &&
        limit_state->rows_seen >= limit_state->row_limit)
      return true;
  }

  if (key.row)
    *lastkey = key;

  return false;
}

void ScanCells::add(Cell &cell, bool own) {
  if (!m_cells)
    m_cells = make_shared<CellsBuilder>();
  m_cells->add(cell, own);
}
