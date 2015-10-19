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

#ifndef Hypertable_RangeServer_KeyDecompressorPrefix_h
#define Hypertable_RangeServer_KeyDecompressorPrefix_h

#include "KeyDecompressor.h"

namespace Hypertable {

  class KeyDecompressorPrefix : public KeyDecompressor {
  public:
    void reset() override;
    const uint8_t *add(const uint8_t *ptr) override;
    bool less_than(SerializedKey serialized_key) override;
    void load(Key &key) override;
  private:
    SerializedKey m_serialized_key;
    DynamicBuffer m_bufs[2];
    const uint8_t *m_current_base;
    bool m_first;
  };

}

#endif // Hypertable_RangeServer_KeyDecompressorPrefix_h
