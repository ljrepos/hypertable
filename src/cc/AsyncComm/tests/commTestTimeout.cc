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

#include <Common/Compat.h>

#include "CommTestThreadFunction.h"

#include <AsyncComm/Comm.h>
#include <AsyncComm/ConnectionManager.h>
#include <AsyncComm/Event.h>
#include <AsyncComm/ReactorFactory.h>

#include <Common/Init.h>
#include <Common/Error.h>
#include <Common/FileUtils.h>
#include <Common/InetAddr.h>
#include <Common/Logger.h>
#include <Common/TestHarness.h>
#include <Common/System.h>
#include <Common/Usage.h>

#include <boost/thread/thread.hpp>

#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <mutex>
#include <thread>

extern "C" {
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
}

using namespace Hypertable;
using namespace Serialization;
using namespace std;

namespace {
  const char *usage[] = {
    "usage: commTestTimout",
    "",
    "This program tests tests the request timeout logic of AsyncComm.",
    0
  };

  const int DEFAULT_PORT = 32998;
  const char *DEFAULT_PORT_ARG = "--port=32998";

  class ServerLauncher {
  public:
    ServerLauncher() {
      if ((m_child_pid = fork()) == 0) {
        execl("./testServer", "./testServer", DEFAULT_PORT_ARG,
              "--delay=120000", (char *)0);
      }
      this_thread::sleep_for(chrono::milliseconds(2000));
    }
    ~ServerLauncher() {
      if (kill(m_child_pid, 9) == -1)
        perror("kill");
    }
    private:
      pid_t m_child_pid;
  };

  /**
   *
   */
  class ResponseHandler : public DispatchHandler {
  public:
    ResponseHandler() { }

    virtual void handle(EventPtr &event_ptr) {
      std::lock_guard<std::mutex> lock(m_mutex);
      if (event_ptr->type == Event::CONNECTION_ESTABLISHED) {
        m_connected = true;
        m_cond.notify_one();
      }
      else if (event_ptr->type == Event::ERROR) {
        HT_INFOF("%s", event_ptr->to_str().c_str());
      }
      else {
        HT_INFOF("%s", event_ptr->to_str().c_str());
        m_connected = true;
        m_cond.notify_one();
      }
    }

    void wait_for_connection() {
      std::unique_lock<std::mutex> lock(m_mutex);
      m_cond.wait(lock, [this](){ return m_connected; });
    }

  private:
    std::mutex m_mutex;
    std::condition_variable m_cond;
    bool m_connected {};
  };

}


int main(int argc, char **argv) {
  struct sockaddr_in addr;
  Comm *comm;
  int error;
  EventPtr event_ptr;
  TestHarness harness("commTestTimeout");
  bool golden = false;
  ResponseHandler *resp_handler = new ResponseHandler();
  DispatchHandlerPtr dhp(resp_handler);
  int diff_exit = 0;

  Config::init(0, 0);

  {
    ServerLauncher slauncher;

    if (argc > 1) {
      if (!strcmp(argv[1], "--golden"))
        golden = true;
      else
        Usage::dump_and_exit(usage);
    }

    srand(8876);

    ReactorFactory::initialize(1);

    InetAddr::initialize(&addr, "localhost", DEFAULT_PORT);

    comm = Comm::instance();

    if ((error = comm->connect(addr, dhp)) != Error::OK)
      return 1;

    resp_handler->wait_for_connection();

    CommHeader header;
    std::string msg;

    header.gid = rand();

    msg = "foo";
    CommBufPtr cbp(new CommBuf(header, encoded_length_str16(msg)));
    cbp->append_str16(msg);
    if ((error = comm->send_request(addr, 5000, cbp, resp_handler)) != Error::OK) {
      HT_ERRORF("Problem sending request - %s", Error::get_text(error));
      return 1;
    }

    msg = "bar";
    cbp = make_shared<CommBuf>(header, encoded_length_str16(msg));
    cbp->append_str16(msg);
    if ((error = comm->send_request(addr, 5000, cbp, resp_handler)) != Error::OK) {
      HT_ERRORF("Problem sending request - %s", Error::get_text(error));
      return 1;
    }

    this_thread::sleep_for(chrono::milliseconds(8000));

    if (!golden)
      diff_exit = harness.validate("commTestTimeout.golden");
  }

  if (!golden)
    quick_exit(diff_exit);

  harness.regenerate_golden_file("commTestTimeout.golden");
  return 0;
}
