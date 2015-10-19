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

#include <ctime>
#include <cmath>

#include "LoadThread.h"

using namespace Hypertable;
using namespace std;

void LoadThread::operator()() {
  unique_lock<mutex> lock(m_state.mutex);
  clock_t start_clocks=0, stop_clocks=0;
  double latency = 0.0;
  double clocks_per_usec = (double)CLOCKS_PER_SEC / 1000000.0;

  try {
    m_mutator.reset(m_table->create_mutator(0, m_mutator_flags,
                                            m_shared_mutator_flush_interval));
  }
  catch (Exception &e) {
    HT_FATAL_OUT << e << HT_END;
  }

  while (!m_state.finished || !m_state.requests.empty()) {
    m_state.cond.wait(lock, [this](){
        return !m_state.requests.empty() || m_state.finished; });
    if (!m_state.requests.empty()) {
      LoadRec *request = m_state.requests.front();

      start_clocks = clock();

      if (request->is_delete)
        m_mutator->set_delete(request->key);
      else
        m_mutator->set(request->key, request->value, request->value_len);
      m_mutator->flush();

      stop_clocks = clock();
      if (stop_clocks < start_clocks)
        latency = ((std::numeric_limits<clock_t>::max() - start_clocks) + stop_clocks) / clocks_per_usec;
      else
        latency = (stop_clocks-start_clocks) / clocks_per_usec;
      m_state.cum_latency += latency;
      m_state.cum_sq_latency += pow(latency,2);
      if (m_state.min_latency == 0 || latency < m_state.min_latency)
        m_state.min_latency = latency;
      if (latency > m_state.max_latency)
        m_state.max_latency = latency;

      m_state.requests.pop_front();
      m_state.garbage.push_back(request);
    }
  }

}
