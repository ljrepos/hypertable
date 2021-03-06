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

package org.hypertable.examples.PerformanceTest;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public class Task {

  public Task() { }

  public Task(long s, long e) {
    start = s;
    end = e;
  }

  public int encodedLength() { return 16; }

  public void encode(ByteBuffer buf) {
    buf.putLong(start);
    buf.putLong(end);
  }

  public void decode(ByteBuffer buf) {
    start = buf.getLong();
    end   = buf.getLong();
  }

  public String toString() {
    return new String("start=" + start + ", end=" + end + ")");
  }

  public long start;
  public long end;
}
