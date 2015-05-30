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

/// @file
/// Definitions of utility functions.
/// This file contains definitions for utility functions for manipulating files
/// in the brokered filesystem.

#include <Common/Compat.h>

#include "Utility.h"

#include <AsyncComm/DispatchHandlerSynchronizer.h>

#include <Common/Error.h>
#include <Common/Logger.h>

#include <cstdio>

using namespace std;
using namespace Hypertable;

namespace {
  const int BUFFER_SIZE = 32768;
}

void FsBroker::Lib::copy(ClientPtr &client, const std::string &from,
                         const std::string &to, int64_t offset) {
  DispatchHandlerSynchronizer sync_handler;
  int32_t from_fd {};
  int32_t to_fd {};

  try {

    from_fd = client->open(from, 0);
    if (offset > 0)
      client->seek(from_fd, offset);

    to_fd = client->create(to, Filesystem::OPEN_FLAG_OVERWRITE, -1, -1, -1);    

    client->read(from_fd, BUFFER_SIZE, &sync_handler);
    client->read(from_fd, BUFFER_SIZE, &sync_handler);
    client->read(from_fd, BUFFER_SIZE, &sync_handler);

    EventPtr event;
    uint8_t *dst;
    uint32_t amount;

    while (sync_handler.wait_for_reply(event)) {

      client->decode_response_pread(event, (const void **)&dst, (uint64_t *)&offset, &amount);

      StaticBuffer send_buf;
      if (amount > 0) {
        send_buf.set(dst, amount, false);
        client->append(to_fd, send_buf);
      }

      if (amount < (uint32_t)BUFFER_SIZE) {
        sync_handler.wait_for_reply(event);
        break;
      }

      client->read(from_fd, BUFFER_SIZE, &sync_handler);
    }

    client->close(from_fd);
    client->close(to_fd);

  }
  catch (Exception &e) {
    if (from_fd)
      client->close(from_fd);
    if (to_fd)
      client->close(to_fd);
    throw;
  }

}


void FsBroker::Lib::copy_from_local(ClientPtr &client, const string &from, const string &to,
                                    int64_t offset) {
  DispatchHandlerSynchronizer sync_handler;
  int32_t fd = 0;
  FILE *fp = 0;
  size_t nread;
  uint8_t *buf;
  StaticBuffer send_buf;

  try {

    if ((fp = fopen(from.c_str(), "r")) == 0)
      HT_THROW(Error::EXTERNAL, strerror(errno));

    if (offset > 0) {
      if (fseek(fp, (long)offset, SEEK_SET) != 0)
        HT_THROW(Error::EXTERNAL, strerror(errno));
    }

    fd = client->create(to, Filesystem::OPEN_FLAG_OVERWRITE, -1, -1, -1);

    // send 3 appends
    for (int i=0; i<3; i++) {
      buf = new uint8_t [BUFFER_SIZE];
      if ((nread = fread(buf, 1, BUFFER_SIZE, fp)) == 0)
        goto done;
      send_buf.set(buf, nread, true);
      client->append(fd, send_buf);
    }

    while (true) {
      buf = new uint8_t [BUFFER_SIZE];
      if ((nread = fread(buf, 1, BUFFER_SIZE, fp)) == 0)
        break;
      send_buf.set(buf, nread, true);
      client->append(fd, send_buf);
    }

  done:
    client->close(fd);

  }
  catch (Exception &e) {
    if (fp)
      fclose(fp);
    throw;
  }
}


void FsBroker::Lib::copy_to_local(ClientPtr &client, const string &from, const string &to,
                                  int64_t offset) {
  DispatchHandlerSynchronizer sync_handler;
  FILE *fp = 0;

  try {

    if ((fp = fopen(to.c_str(), "w+")) == 0)
      HT_THROW(Error::EXTERNAL, strerror(errno));

    int32_t fd = client->open(from, 0);

    if (offset > 0)
      client->seek(fd, offset);

    client->read(fd, BUFFER_SIZE, &sync_handler);
    client->read(fd, BUFFER_SIZE, &sync_handler);
    client->read(fd, BUFFER_SIZE, &sync_handler);

    EventPtr event;
    const uint8_t *dst;
    uint32_t amount;

    while (sync_handler.wait_for_reply(event)) {

      client->decode_response_pread(event, (const void **)&dst, (uint64_t *)&offset, &amount);

      if (amount > 0) {
        if (fwrite(dst, amount, 1, fp) != 1)
          HT_THROW(Error::EXTERNAL, strerror(errno));
      }

      if (amount < (uint32_t)BUFFER_SIZE) {
        sync_handler.wait_for_reply(event);
        break;
      }

      client->read(fd, BUFFER_SIZE, &sync_handler);
    }

    client->close(fd);

    fclose(fp);

  }
  catch (Exception &e) {
    if (fp)
      fclose(fp);
    throw;
  }
  
}

