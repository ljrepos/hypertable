/**
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

#include <Common/Compat.h>

#include "CommBuf.h"
#include "Event.h"
#include "Protocol.h"

#include <Common/Error.h>
#include <Common/Logger.h>
#include <Common/Serialization.h>

#include <cassert>
#include <iostream>

using namespace Hypertable;
using namespace Serialization;
using namespace std;

int32_t Protocol::response_code(const Event *event) {
  if (event->type == Event::ERROR)
    return event->error;

  const uint8_t *msg = event->payload;
  size_t remaining = event->payload_len;

  try { return decode_i32(&msg, &remaining); }
  catch (Exception &e) { return e.code(); }
}


String Protocol::string_format_message(const Event *event) {
  int error = Error::OK;

  if (event == 0)
    return String("NULL event");

  const uint8_t *msg = event->payload;
  size_t remaining = event->payload_len;

  if (event->type != Event::MESSAGE)
    return event->to_str();

  try {
    error = decode_i32(&msg, &remaining);

    if (error == Error::OK)
      return Error::get_text(error);

    uint16_t len;
    const char *str = decode_str16(&msg, &remaining, &len);

    return String(str, len > 150 ? 150 : len);
  }
  catch (Exception &e) {
    return format("%s - %s", e.what(), Error::get_text(e.code()));
  }
}


CommBufPtr
Protocol::create_error_message(CommHeader &header, int error, const char *msg){
  CommBufPtr cbuf = make_shared<CommBuf>(header, 4 + encoded_length_str16(msg));
  cbuf->append_i32(error);
  cbuf->append_str16(msg);
  return cbuf;
}


