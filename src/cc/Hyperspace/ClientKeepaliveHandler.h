/**
 * Copyright (C) 2007 Doug Judd (Zvents, Inc.)
 * 
 * This file is part of Hypertable.
 * 
 * Hypertable is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or any later version.
 * 
 * Hypertable is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef HYPERSPACE_CLIENTKEEPALIVEHANDLER_H
#define HYPERSPACE_CLIENTKEEPALIVEHANDLER_H

#include <boost/thread/mutex.hpp>

#include "Common/Properties.h"

#include "AsyncComm/Comm.h"
#include "AsyncComm/DispatchHandler.h"

#include "ClientSessionState.h"
#include "ClientConnectionHandler.h"

namespace Hyperspace {

  class SessionCallback;

  /**
   * 
   */
  class ClientKeepaliveHandler : public DispatchHandler {

  public:
    ClientKeepaliveHandler(Comm *comm, PropertiesPtr &propsPtr, Hyperspace::SessionCallback *sessionCallback, ClientSessionStatePtr &sessionStatePtr);
    virtual void handle(EventPtr &eventPtr);

  private:
    boost::mutex       mMutex;
    boost::xtime       mLastKeepAliveSendTime;
    Comm *mComm;
    Hyperspace::SessionCallback *mSessionCallback;
    uint32_t mLeaseInterval;
    uint32_t mKeepAliveInterval;
    struct sockaddr_in mMasterAddr;
    struct sockaddr_in mLocalAddr;
    bool mVerbose;
    ClientSessionStatePtr mSessionStatePtr;
    uint32_t mSessionId;
    ClientConnectionHandler *mConnHandler;
  };

}

#endif // HYPERSPACE_CLIENTKEEPALIVEHANDLER_H

