/** -*- c++ -*-
 * Copyright (C) 2008 Doug Judd (Zvents, Inc.)
 *
 * This file is part of Hypertable.
 *
 * Hypertable is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of the
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

#include "Common/Compat.h"
#include <cassert>
#include <iostream>

#include "Key.h"

using namespace Hypertable;
using namespace std;

namespace {
  const char end_row_chars[3] = { (char)0xff, (char)0xff, 0 };
  const char end_root_row_chars[5] = { '0', ':', (char)0xff, (char)0xff, 0 };

  size_t write_key(uint8_t *buf, uint8_t control, uint8_t flag, const char *row, uint8_t column_family_code, const char *column_qualifier) {
    uint8_t *ptr = buf;
    *ptr++ = control;
    strcpy((char *)ptr, row);
    ptr += strlen(row) + 1;
    *ptr++ = column_family_code;
    if (column_qualifier != 0) {
      strcpy((char *)ptr, column_qualifier);
      ptr += strlen(column_qualifier) + 1;
    }
    else
      *ptr++ = 0;
    *ptr++ = flag;
    return ptr-buf;
  }
}



namespace Hypertable {

  const char *Key::END_ROW_MARKER = (const char *)end_row_chars;
  const char *Key::END_ROOT_ROW   = (const char *)end_root_row_chars;

  ByteString create_key(uint8_t flag, const char *row, uint8_t column_family_code, const char *column_qualifier, int64_t timestamp, int64_t revision) {
    size_t len = 1 + strlen(row) + 4;
    uint8_t control = 0;

    if (timestamp == AUTO_ASSIGN)
      control = Key::CONTROL_MASK_AUTO_TIMESTAMP;
    else if (timestamp != TIMESTAMP_NULL) {
      len += 8;
      control = Key::CONTROL_MASK_HAVE_TIMESTAMP;
    }

    if (revision != AUTO_ASSIGN) {
      len += 8;
      control |= Key::CONTROL_MASK_HAVE_REVISION;
    }

    uint8_t *ptr = new uint8_t [len + Serialization::encoded_length_vi32(len)];  // !!! could probably just make this 6
    ByteString bs(ptr);
    Serialization::encode_vi32(&ptr, len);
    ptr += write_key(ptr, control, flag, row, column_family_code, column_qualifier);

    if (control & Key::CONTROL_MASK_HAVE_TIMESTAMP)
      Key::encode_ts64(&ptr, timestamp);

    if (control & Key::CONTROL_MASK_HAVE_REVISION)
      Key::encode_ts64(&ptr, revision);

    return bs;
  }

  void create_key_and_append(DynamicBuffer &dst_buf, uint8_t flag, const char *row, uint8_t column_family_code, const char *column_qualifier, int64_t timestamp, int64_t revision) {
    size_t len = 1 + strlen(row) + 4;
    uint8_t control = 0;

    if (column_qualifier)
      len += strlen(column_qualifier);

    if (timestamp == AUTO_ASSIGN)
      control = Key::CONTROL_MASK_AUTO_TIMESTAMP;
    else if (timestamp != TIMESTAMP_NULL) {
      len += 8;
      control = Key::CONTROL_MASK_HAVE_TIMESTAMP;
      if (timestamp == revision)
	control |= Key::CONTROL_MASK_SHARED;
    }

    if (revision != AUTO_ASSIGN) {
      if (!(control & Key::CONTROL_MASK_SHARED))
	len += 8;
      control |= Key::CONTROL_MASK_HAVE_REVISION;
    }

    dst_buf.ensure(len + 6);
    Serialization::encode_vi32(&dst_buf.ptr, len);
    dst_buf.ptr += write_key(dst_buf.ptr, control, flag, row, column_family_code, column_qualifier);

    if (control & Key::CONTROL_MASK_HAVE_TIMESTAMP)
      Key::encode_ts64(&dst_buf.ptr, timestamp);

    if ((control & Key::CONTROL_MASK_HAVE_REVISION) && !(control & Key::CONTROL_MASK_SHARED))
      Key::encode_ts64(&dst_buf.ptr, revision);

  }

  void create_key_and_append(DynamicBuffer &dst_buf, const char *row) {
    uint8_t control = Key::CONTROL_MASK_HAVE_REVISION | Key::CONTROL_MASK_HAVE_TIMESTAMP | Key::CONTROL_MASK_SHARED;
    size_t len = 13 + strlen(row);
    dst_buf.ensure(len + 6);
    Serialization::encode_vi32(&dst_buf.ptr, len);
    dst_buf.ptr += write_key(dst_buf.ptr, control, 0, row, 0, 0);
    Key::encode_ts64(&dst_buf.ptr, 0);
  }

  Key::Key(SerializedKey key) {
    load(key);
  }

  /**
   * TODO: Re-implement below function in terms of this function
   */
  bool Key::load(SerializedKey key) {

    serial = key;

    size_t len = Serialization::decode_vi32(&key.ptr);

    length = len + (key.ptr - serial.ptr);

    const uint8_t *end_ptr = key.ptr + len;

    control = *key.ptr++;

    row = (const char *)key.ptr;

    while (key.ptr < end_ptr && *key.ptr != 0)
      key.ptr++;
    key.ptr++;
    if (key.ptr >= end_ptr) {
      cerr << "row decode overrun" << endl;
      return false;
    }

    column_family_code = *key.ptr++;
    column_qualifier = (const char *)key.ptr;

    while (key.ptr < end_ptr && *key.ptr != 0)
      key.ptr++;
    key.ptr++;
    if (key.ptr >= end_ptr) {
      cerr << "qualifier decode overrun" << endl;
      return false;
    }

    flag_ptr = key.ptr;
    flag = *key.ptr++;

    if (control & CONTROL_MASK_HAVE_TIMESTAMP) {
      timestamp = decode_ts64((const uint8_t **)&key.ptr);
      if (control & CONTROL_MASK_SHARED) {
	revision = timestamp;
	assert(key.ptr == end_ptr);
	return true;
      }
    }
    else {
      if (control & CONTROL_MASK_AUTO_TIMESTAMP)
	timestamp = AUTO_ASSIGN;
      else
	timestamp = TIMESTAMP_NULL;
    }

    if (control & CONTROL_MASK_HAVE_REVISION)
      revision = decode_ts64((const uint8_t **)&key.ptr);
    else
      revision = AUTO_ASSIGN;

    assert(key.ptr == end_ptr);

    return true;
  }


  std::ostream &operator<<(std::ostream &os, const Key &key) {
    bool got = false;
    os << "control=(";
    if (key.control & Key::CONTROL_MASK_HAVE_REVISION) {
      os << "REV";
      got = true;
    }
    if (key.control & Key::CONTROL_MASK_HAVE_TIMESTAMP) {
      os << ((got) ? "|TS" : "TS");
      got = true;
    }
    if (key.control & Key::CONTROL_MASK_AUTO_TIMESTAMP) {
      os << ((got) ? "|AUTO" : "AUTO");
      got = true;
    }
    if (key.control & Key::CONTROL_MASK_SHARED) {
      os << ((got) ? "|SHARED" : "SHARED");
      got = true;
    }
    os << ") row='" << key.row << "' ";
    if (key.flag == FLAG_DELETE_ROW)
      os << "ts=" << key.timestamp << " rev=" << key.revision << " DELETE";
    else {
      os << "family=" << (int)key.column_family_code;
      if (key.column_qualifier)
        os << " qualifier='" << key.column_qualifier << "'";
      os << " ts=" << key.timestamp;
      os << " rev=" << key.revision;
      if (key.flag == FLAG_DELETE_CELL)
        os << " DELETE";
      else if (key.flag == FLAG_DELETE_COLUMN_FAMILY)
        os << " DELETE_COLUMN_FAMILY";
    }
    return os;
  }


}

