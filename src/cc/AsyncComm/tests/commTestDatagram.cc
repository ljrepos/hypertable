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

#include "CommTestDatagramThreadFunction.h"

#include <AsyncComm/Comm.h>
#include <AsyncComm/ConnectionManager.h>
#include <AsyncComm/Event.h>
#include <AsyncComm/ReactorFactory.h>

#include <Common/Init.h>
#include <Common/Error.h>
#include <Common/FileUtils.h>
#include <Common/TestHarness.h>
#include <Common/StringExt.h>
#include <Common/System.h>
#include <Common/Usage.h>

#include <boost/thread/thread.hpp>

#include <chrono>
#include <cstdlib>
#include <thread>

extern "C" {
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
}

using namespace Hypertable;
using namespace std;

namespace {
  const char *usage[] = {
    "usage: commTestDatagram",
    "",
    "This program tests the UDP portion of AsyncComm.  It runs the echo",
    "server (testServer) in UDP mode, and then spawns two threads that",
    "read ./words, one line at a time, and sends these",
    "lines to the echo server in UDP packets and writes the replies to",
    "temporary output files.  These files are then diff'ed against the",
    "original.",
    0
  };

  const int DEFAULT_PORT = 32998;
  const char *DEFAULT_PORT_ARG = "--port=32998";

  class ServerLauncher {
  public:
    ServerLauncher() {
      if ((m_child_pid = fork()) == 0) {
        execl("./testServer", "./testServer", DEFAULT_PORT_ARG, "--app-queue",
              "--udp", (char *)0);
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

}


int main(int argc, char **argv) {
  boost::thread  *thread1, *thread2;
  struct sockaddr_in addr;
  ServerLauncher slauncher;

  Config::init(argc, argv);

  if (argc != 1)
    Usage::dump_and_exit(usage);

  srand(8876);

  System::initialize(System::locate_install_dir(argv[0]));
  ReactorFactory::initialize(1);

  memset(&addr, 0, sizeof(struct sockaddr_in));
  {
    struct hostent *he = gethostbyname("localhost");
    if (he == 0) {
      herror("gethostbyname()");
      return 1;
    }
    memcpy(&addr.sin_addr.s_addr, he->h_addr_list[0], sizeof(uint32_t));
  }
  addr.sin_family = AF_INET;
  addr.sin_port = htons(DEFAULT_PORT);

  this_thread::sleep_for(chrono::milliseconds(2000));

  CommTestDatagramThreadFunction thread_func(Comm::instance(), addr, "./words");

  thread_func.set_output_file("commTestDatagram.output.1");
  thread_func.set_receive_port(DEFAULT_PORT + 1);
  thread1 = new boost::thread(thread_func);

  thread_func.set_output_file("commTestDatagram.output.2");
  thread_func.set_receive_port(DEFAULT_PORT + 2);
  thread2 = new boost::thread(thread_func);

  thread1->join();
  thread2->join();

  String tmp_file = (String)"/tmp/commTestDatagram" + (int)getpid();
  String cmd_str = (String)"head -" + (int)MAX_MESSAGES + " ./words > "
      + tmp_file  + " ; diff " + tmp_file + " commTestDatagram.output.1";

  if (system(cmd_str.c_str()))
    return 1;

  cmd_str = (String)"unlink " + tmp_file;
  if (system(cmd_str.c_str()))
    return 1;

  if (system("diff commTestDatagram.output.1 commTestDatagram.output.2"))
    return 1;

  ReactorFactory::destroy();

  delete thread1;
  delete thread2;

  return 0;
}
