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

/** @file
 * Definitions for DispatchHandlerOperation.
 * This file contains definitions for DispatchHandlerOperation, a base class
 * for dispatch handlers used by operations that issue a request to a set of
 * range servers.
 */

#include <Common/Compat.h>

#include "Context.h"
#include "DispatchHandlerOperation.h"

#include <AsyncComm/Protocol.h>

#include <Common/Error.h>
#include <Common/Logger.h>

using namespace Hypertable;

DispatchHandlerOperation::DispatchHandlerOperation(ContextPtr &context)
  : m_context(context), m_rsclient(Comm::instance()), m_outstanding(0), m_error_count(0) {
}


void DispatchHandlerOperation::start(StringSet &locations) {

  m_results.clear();
  m_error_count = 0;
  m_locations = locations;
  m_outstanding = locations.size();

  for (StringSet::iterator iter = m_locations.begin(); iter != m_locations.end(); ++iter) {
    try {
      start(*iter);
    }
    catch (Exception &e) {
      HT_INFOF("%s - %s", Error::get_text(e.code()), e.what());
      if (e.code() == Error::COMM_NOT_CONNECTED ||
          e.code() == Error::COMM_INVALID_PROXY ||
          e.code() == Error::COMM_BROKEN_CONNECTION) {
        lock_guard<mutex> lock(m_mutex);
        Result result(*iter);
        m_outstanding--;
        result.error = e.code();
        result.msg = "Send error";
        m_results.insert(result);
        m_error_count++;
      }
    }
  }
}


/**
 *
 */
void DispatchHandlerOperation::handle(EventPtr &event) {
  lock_guard<mutex> lock(m_mutex);

  if (m_events.count(event) > 0) {
    HT_INFOF("Skipping second event - %s", event->to_str().c_str());
    return;
  }

  HT_ASSERT(m_outstanding > 0);
  m_events.insert(event);
  m_outstanding--;
  if (m_outstanding == 0)
    m_cond.notify_all();
}


void DispatchHandlerOperation::process_events() {
  RangeServerConnectionPtr rsc;

  for (const auto &event : m_events) {

    if (m_context->rsc_manager->find_server_by_local_addr(event->addr, rsc)) {
      Result result(rsc->location());
      if (event->type == Event::MESSAGE) {
        if ((result.error = Protocol::response_code(event)) != Error::OK) {
          m_error_count++;
          result.msg = Protocol::string_format_message(event);
          m_results.insert(result);
        }
      }
      else {
        m_error_count++;
        result.error = event->error;
        result.msg = "";
        m_results.insert(result);
      }
    }
    else
      HT_WARNF("Couldn't locate connection object for %s",
               InetAddr(event->addr).format().c_str());

    result_callback(event);
  }
}


bool DispatchHandlerOperation::wait_for_completion() {
  unique_lock<mutex> lock(m_mutex);
  m_cond.wait(lock, [this](){ return m_outstanding == 0; });
  process_events();
  return m_error_count == 0;
}


void DispatchHandlerOperation::get_results(std::set<Result> &results) {
  lock_guard<mutex> lock(m_mutex);
  results = m_results;
}
