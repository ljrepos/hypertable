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
#include <AsyncComm/DispatchHandler.h>
#include <AsyncComm/Event.h>

#include <Common/Error.h>
#include <Common/Serialization.h>

#include <boost/thread/thread.hpp>

#include <chrono>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>

using namespace std;
using namespace Hypertable;
using namespace Serialization;

namespace {

  class ResponseHandler : public DispatchHandler {

  public:

    ResponseHandler() { }

    virtual void handle(EventPtr &event_ptr) {
      std::lock_guard<std::mutex> lock(m_mutex);
      if (event_ptr->type == Event::MESSAGE) {
        m_queue.push(event_ptr);
        m_cond.notify_one();
      }
      else {
        event_ptr->display();
        m_connected = false;
        m_cond.notify_one();
      }
    }

    bool get_response(EventPtr &event_ptr) {
      std::unique_lock<std::mutex> lock(m_mutex);
      while (m_queue.empty()) {
        m_cond.wait(lock);
        if (m_connected == false)
          return false;
      }
      event_ptr = m_queue.front();
      m_queue.pop();
      return true;
    }

  private:
    std::queue<EventPtr> m_queue;
    std::mutex m_mutex;
    std::condition_variable m_cond;
    bool m_connected {true};
  };
}


/**
 *
 */
void CommTestThreadFunction::operator()() {
  CommHeader header;
  int error;
  EventPtr event_ptr;
  int outstanding = 0;
  int max_outstanding = 50;
  string line;
  ifstream infile(m_input_file);
  ofstream outfile(m_output_file);
  const char *str;
  int nsent = 0;
  ResponseHandler *resp_handler = new ResponseHandler();
  DispatchHandlerPtr dhp(resp_handler);
  const uint8_t *decode_ptr;
  size_t decode_remain;

  header.gid = rand();

  if (infile.is_open()) {
    while (!infile.eof() && nsent < MAX_MESSAGES) {
      getline (infile,line);
      if (infile.fail())
        break;

      CommBufPtr cbp( new CommBuf(header, encoded_length_str16(line)) );
      cbp->append_str16(line);
      int retries = 0;
      while ((error = m_comm->send_request(m_addr, 30000, cbp, resp_handler)) != Error::OK) {
        if (error == Error::COMM_NOT_CONNECTED) {
          if (retries == 5) {
            HT_ERROR("Connection timeout.");
            return;
          }
          this_thread::sleep_for(chrono::milliseconds(1000));
          retries++;
        }
        else {
          HT_ERRORF("CommEngine::send_message returned '%s'",
                    Error::get_text(error));
          return;
        }
      }
      outstanding++;

      if (outstanding  > max_outstanding) {
        if (!resp_handler->get_response(event_ptr))
          break;
        try {
          decode_ptr = event_ptr->payload;
          decode_remain = event_ptr->payload_len;
          str = decode_str16(&decode_ptr, &decode_remain);
          if (*str != 0)
            outfile << str << endl;
          else
            outfile << endl;
        }
        catch (Exception &e) {
          outfile <<"Error: "<< e << endl;
        }
        outstanding--;
      }
      nsent++;
    }
    infile.close();
  }
  else {
    HT_ERRORF("Unable to open file '%s' : %s", m_input_file, strerror(errno));
    return;
  }

  while (outstanding > 0 && resp_handler->get_response(event_ptr)) {
    try {
      decode_ptr = event_ptr->payload;
      decode_remain = event_ptr->payload_len;
      str = decode_str16(&decode_ptr, &decode_remain);
      if (*str != 0)
        outfile << str << endl;
      else
        outfile << endl;
    }
    catch (Exception &e) {
      outfile <<"Error: "<< e << endl;
    }
    //cout << "out = " << outstanding << endl;
    outstanding--;
  }
}
