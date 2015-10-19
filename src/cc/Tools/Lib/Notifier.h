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

#ifndef Tools_Lib_Notifier_h
#define Tools_Lib_Notifier_h

#include <AsyncComm/Comm.h>
#include <AsyncComm/CommAddress.h>
#include <AsyncComm/CommBuf.h>

#include <Common/Error.h>
#include <Common/InetAddr.h>

#include <cstdio>
#include <memory>
#include <string>

namespace Hypertable {

  /*
   * Helper class which sends notification to specified address.
   */
  class Notifier {
  public:
    Notifier(const char *addr_str) {
      DispatchHandlerPtr null_handler(0);
      InetAddr inet_addr;
      m_comm = Comm::instance();
      if (!InetAddr::initialize(&inet_addr, addr_str)) {
        exit(EXIT_FAILURE);
      }
      m_addr = inet_addr;
      InetAddr::initialize(&inet_addr, INADDR_ANY, 0);
      m_send_addr = inet_addr;
      m_comm->create_datagram_receive_socket(m_send_addr, 0x10, null_handler);
    }

    Notifier() { }

    void notify() {
      if (m_comm) {
        int error;
        CommHeader header(0);
        CommBufPtr cbp(new CommBuf(header, 0));
        if ((error = m_comm->send_datagram(m_addr, m_send_addr, cbp))
            != Error::OK) {
          HT_ERRORF("Problem sending datagram - %s", Error::get_text(error));
          exit(EXIT_FAILURE);
        }
      }
    }

  private:
    Comm *m_comm {};
    CommAddress m_addr;
    CommAddress m_send_addr;
  };

  typedef std::shared_ptr<Notifier> NotifierPtr;

}

#endif // Tools_Lib_Notifier_h
