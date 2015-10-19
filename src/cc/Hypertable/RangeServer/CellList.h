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

#ifndef Hypertable_RangeServer_CellList_h
#define Hypertable_RangeServer_CellList_h

#include "ScanContext.h"

#include <Hypertable/Lib/Key.h>

#include <Common/ByteString.h>
#include <Common/StlAllocator.h>
#include <Common/StringExt.h>

namespace Hypertable {

  class CellListScanner;
  typedef std::shared_ptr<CellListScanner> CellListScannerPtr;

  /**
   * Abstract base class for cell lists (sorted lists of key/value
   * pairs).  Cell lists include cell stores and cell caches.
   */
  class CellList {
  public:
    virtual ~CellList() { return; }

    /**
     * Inserts a key/value pair into the cell list.
     *
     * @param key key object
     * @param value ByteString representing value
     */
    virtual void add(const Key &key, const ByteString value) = 0;

    /**
     * Creates a scanner on this cell list.
     *
     * @param scan_ctx smart pointer to scan context
     * @return pointer to newly allocated scanner
     */
    virtual CellListScannerPtr
    create_scanner(ScanContext *scan_ctx) { return CellListScannerPtr(); }

    typedef std::pair<const char *, int64_t> SplitRowDataValue;
    typedef StlAllocator<SplitRowDataValue> SplitRowDataAlloc;
    typedef std::map<const char *, int64_t, LtCstr,
                     SplitRowDataAlloc> SplitRowDataMapT;

    /** Populates <code>split_row_data</code> with unique row and count
     * estimates for this list.
     * @param split_row_data Reference to accumulator map holding estimate
     * of unique rows and counts.
     * @note <code>split_row_data</code> should not be cleared
     */
    virtual void split_row_estimate_data(SplitRowDataMapT &split_row_data) {
      HT_FATAL("Not Implemented!");
    }

    /**
     * Returns the start row of this cell list.  This value is used to restrict
     * the start range of the cell list to values that are greater than this
     * row key.  It is used in cell stores to allow them to be shared after
     * a split.
     *
     * @return the row key of the start row (not inclusive)
     */
    virtual const char *get_start_row() { return m_start_row.c_str(); }

    /**
     * Returns the end row of this cell list.  This value is used to restrict
     * the end range of the cell list to values that are less than or equal to
     * this row key.  It is used in cell stores to allow them to be shared after
     * a split.
     *
     * @return the row key of the end row (inclusive)
     */
    virtual const char *get_end_row() { return m_end_row.c_str(); }

  protected:
    std::string m_start_row;
    std::string m_end_row;
  };

}

#endif // Hypertable_RangeServer_CellList_h
