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

#ifndef Hypertable_RangeServer_KeyCompressor_h
#define Hypertable_RangeServer_KeyCompressor_h

#include <Hypertable/Lib/Key.h>

#include <memory>

namespace Hypertable {

  namespace KeyCompressionType {
    enum { NONE=0, PREFIX=1 };
  }

  class KeyCompressor {
  public:
    virtual void reset() = 0;
    virtual void add(const Key &key) = 0;
    virtual size_t length() = 0;
    virtual size_t length_uncompressed() = 0;
    virtual void write(uint8_t *buf) = 0;
    virtual void write_uncompressed(uint8_t *buf) = 0;
  };

  typedef std::shared_ptr<KeyCompressor> KeyCompressorPtr;

}

#endif // Hypertable_RangeServer_KeyCompressor_h
