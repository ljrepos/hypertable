/** -*- c++ -*-
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

#include "Common/Compat.h"

#include "CommandInterpreter.h"

#include <cassert>

using namespace Hypertable;
using namespace std;

CommandInterpreter::CommandInterpreter()
  : m_timestamp_output_format(TIMESTAMP_FORMAT_DEFAULT), m_silent(false),
    m_test_mode(false) {
}


void
CommandInterpreter::set_timestamp_output_format(const String &format) {
  if (format == "default")
    m_timestamp_output_format = TIMESTAMP_FORMAT_DEFAULT;
  else if (format == "nanos")
    m_timestamp_output_format = TIMESTAMP_FORMAT_NANOS;
  else if (format == "nanoseconds")
    m_timestamp_output_format = TIMESTAMP_FORMAT_NANOS;
  else if (format == "usecs")  // backward compatibilty
    m_timestamp_output_format = TIMESTAMP_FORMAT_NANOS;
  else {
    assert(!"invalid timestamp format");
  }
}

