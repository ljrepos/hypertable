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

/** @file
 * Declarations for DispatchHandlerSynchronizer.
 * This file contains type declarations for DispatchHandlerSynchronizer, a class
 * used to synchronzie with response messages.
 */

#ifndef AsyncComm_DispatchHandlerSynchronizer_h
#define AsyncComm_DispatchHandlerSynchronizer_h

#include "DispatchHandler.h"
#include "Event.h"

#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>

namespace Hypertable {

  /** @addtogroup AsyncComm
   *  @{
   */

  /** DispatchHandler class used to synchronize with response messages.
   * This class is a specialization of DispatchHandler that is used to
   * synchronize with responses resulting from previously sent request messages.
   * It contains a queue of events (response events) and a condition variable
   * that gets signalled when an event gets put on the queue.
   *
   * Example usage:
   *
   * <pre>
   * {
   *   DispatchHandlerSynchronizer sync_handler;
   *   EventPtr event;
   *   CommBufPtr cbp(... create protocol message here ...);
   *   if ((error = m_comm->send_request(m_addr, cbp, &sync_handler))
   *       != Error::OK) {
   *      // log error message here ...
   *      return error;
   *   }
   *   if (!sync_handler.wait_for_reply(event))
   *       // log error message here ...
   *   error = (int)Protocol::response_code(event);
   *   return error;
   * } </pre>
   *
   */
  class DispatchHandlerSynchronizer : public DispatchHandler {

  public:
    /** Constructor.  Initializes state.
     */
    DispatchHandlerSynchronizer();

    virtual ~DispatchHandlerSynchronizer() {
    }

    /** Event Dispatch method.  This gets called by the AsyncComm layer when an
     * event occurs in response to a previously sent request that was supplied
     * with this dispatch handler.  It pushes the event onto the event queue and
     * signals (notify_one) the condition variable.
     *
     * @param event Smart pointer to event object
     */
    virtual void handle(EventPtr &event);

    /** This method is used by a client to synchronize.  The client
     * sends a request via the AsyncComm layer with this object
     * as the dispatch handler.  It then calls this method to
     * wait for the response (or timeout event).  This method
     * just blocks on the condition variable until the event
     * queue is non-empty and then removes and returns the head of the
     * queue.
     *
     * @param event Smart pointer to event object
     * @return true if next returned event is type MESSAGE and contains
     *         status Error::OK, false otherwise
     */
    bool wait_for_reply(EventPtr &event);

    /// Waits for CONNECTION_ESTABLISHED event.
    /// This function waits for an event to arrive on #m_receive_queue and if it
    /// is an ERROR event, it throws an exception, if it is a DISCONNECT event
    /// it returns <i>false</i>, and if it is a CONNECTION_ESTABLISHED event,
    /// it returns <i>true</i>.
    /// @return <i>true</i> if CONNECTION_ESTABLISHED event received,
    /// <i>false</i> if DISCONNECT event received.
    /// @throws Exception with code set to ERROR event error code.
    bool wait_for_connection();

  private:

    /// Mutex for serializing concurrent access
    std::mutex m_mutex;

    /// Condition variable for signalling change in queue state
    std::condition_variable m_cond;

    /// Event queue
    std::queue<EventPtr> m_receive_queue;

  };

  /// Shared smart pointer to DispatchHandlerSynchronizer
  typedef std::shared_ptr<DispatchHandlerSynchronizer> DispatchHandlerSynchronizerPtr;

  /** @}*/
}


#endif // AsyncComm_DispatchHandlerSynchronizer_h
