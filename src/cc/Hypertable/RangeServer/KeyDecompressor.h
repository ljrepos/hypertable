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

#ifndef Hypertable_RangeServer_KeyDecompressor_h
#define Hypertable_RangeServer_KeyDecompressor_h

#include <Hypertable/Lib/Key.h>
#include <Hypertable/Lib/SerializedKey.h>

namespace Hypertable {

  class KeyDecompressor {
  public:
    virtual ~KeyDecompressor() {}
    virtual void reset() = 0;
    virtual const uint8_t *add(const uint8_t *ptr) = 0;
    virtual bool less_than(SerializedKey serialized_key) = 0;
    virtual void load(Key &key) = 0;
  };

}

#endif // Hypertable_RangeServer_KeyDecompressor_h
