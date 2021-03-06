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

#include "AcknowledgeLoad.h"

#include <Hypertable/Lib/RangeServer/Response/Parameters/AcknowledgeLoad.h>

#include <AsyncComm/CommBuf.h>
#include <AsyncComm/CommHeader.h>

using namespace Hypertable;
using namespace Hypertable::RangeServer::Response::Callback;
using namespace std;

int AcknowledgeLoad::response(const map<QualifiedRangeSpec, int32_t> &error_map) {
  CommHeader header;
  header.initialize_from_request_header(m_event->header);
  Lib::RangeServer::Response::Parameters::AcknowledgeLoad params(error_map);
  CommBufPtr cbuf(new CommBuf( header, 4 + params.encoded_length()));
  cbuf->append_i32(Error::OK);
  params.encode(cbuf->get_data_ptr_address());
  return m_comm->send_response(m_event->addr, cbuf);
}
