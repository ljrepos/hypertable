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

package org.hypertable.FsBroker.Lib;

import java.net.ProtocolException;
import java.util.logging.Logger;
import org.hypertable.AsyncComm.ApplicationHandler;
import org.hypertable.AsyncComm.Comm;
import org.hypertable.AsyncComm.Event;
import org.hypertable.AsyncComm.ResponseCallback;
import org.hypertable.Common.Error;
import org.hypertable.Common.Serialization;

public class RequestHandlerSync extends ApplicationHandler {

  static final Logger log = Logger.getLogger("org.hypertable");

  static final byte VERSION = 1;

  public RequestHandlerSync(Comm comm, Broker broker, Event event) {
    super(event);
    mComm = comm;
    mBroker = broker;
  }

  public void run() {

    ResponseCallback cb = new ResponseCallback(mComm, mEvent);

    try {

      if (mEvent.payload.remaining() < 2)
        throw new ProtocolException("Truncated message");

      int version = (int)mEvent.payload.get();
      if (version != VERSION)
        throw new ProtocolException("Sync parameters version mismatch, expected " +
                                    VERSION + ", got " + version);

      int encoding_length = Serialization.DecodeVInt32(mEvent.payload);
      int start_position = mEvent.payload.position();

      int fd = mEvent.payload.getInt();

      if ((mEvent.payload.position() - start_position) < encoding_length)
        mEvent.payload.position(start_position + encoding_length);

      mBroker.Sync(cb, fd);

    }
    catch (Exception e) {
      int error = cb.error(Error.PROTOCOL_ERROR, e.getMessage());
      log.severe("Protocol error (SYNC) - " + e.getMessage());
      if (error != Error.OK)
        log.severe("Problem sending (SYNC) error back to client - "
                   + Error.GetText(error));
    }
  }

  private Comm mComm;
  private Broker mBroker;
}
