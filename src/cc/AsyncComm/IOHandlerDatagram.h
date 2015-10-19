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
 * Declarations for IOHandlerDatagram.
 * This file contains type declarations for IOHandlerDatagram, a class for
 * processing I/O events for datagram sockets.
 */

#ifndef AsyncComm_IOHandlerDatagram_h
#define AsyncComm_IOHandlerDatagram_h

#include "CommBuf.h"
#include "IOHandler.h"

#include <list>
#include <utility>

extern "C" {
#include <netdb.h>
#include <string.h>
}

namespace Hypertable {

  /** @addtogroup AsyncComm
   *  @{
   */

  /** I/O handler for datagram (UDP) sockets.
   */
  class IOHandlerDatagram : public IOHandler {

  public:

    /** Constructor.  This method initializes the handler by allocating
     * a receive buffer and copying the locally bound address to #m_addr.
     * @param sd Socket descriptor bound to an address
     * @param dhp Default dispatch handler
     */
    IOHandlerDatagram(int sd, const DispatchHandlerPtr &dhp) : IOHandler(sd, dhp) {
      m_message = new uint8_t [65536];
      memcpy(&m_addr, &m_local_addr, sizeof(InetAddr));
    }

    /** Destructor. */
    virtual ~IOHandlerDatagram() { delete [] m_message; }

    /** Sends a message.  This method pushes the message pointed to by
     * <code>cbp</code> onto the send queue, flushes the send queue with a call
     * to #flush_send_queue, and then updates polling interest depending on
     * the state of the send queue.
     * @param addr Remote address to send message to
     * @param cbp Pointer to message to send
     * @return Error::OK on success, or Error::COMM_SEND_ERROR on send error
     */
    int send_message(const InetAddr &addr, CommBufPtr &cbp);

    /** Flushes send queue.  This method tries to write all messages on the
     * send queue.  If a write succeeds, but writes fewer bytes than requested,
     * that implies the send buffer is full.  The send queue holds a list of
     * CommBuf objects (and associated destination addresses) that contain
     * <i>next write</i> pointers that are updated by this method and allow it to
     * pick up where it left off in the event of a short write.
     * @return Error::OK on success, or Error::COMM_SEND_ERROR on send error
     */
    int flush_send_queue();

    /** Handle <code>poll()</code> interface events.
     * This method is called by its reactor thread to handle I/O events.
     * It handles <code>POLLOUT</code> events with a call to
     * #handle_write_readiness.  If #handle_write_readiness returns <i>true</i>
     * the handler is disconnected with a call to handle_disconnect() and
     * <i>true</i> is returned.  <code>POLLIN</code> events are handled in
     * a loop by reading messages off the socket in their entirety with a call
     * to <code>recvfrom</code>.  The message buffer is 65536 bytes in size
     * and therefore this method can only handle messages of that size or
     * less.  Messages are delivered to the application by creating an Event
     * object, initializing it with the message header and
     * <code>arrival_time</code>, and delivering it with the default dispatch
     * handler.  <code>POLLERR</code> and errors are handled by decomissioning
     * the handler and delivering an Event::ERROR event to the application.
     * @warning This method assumes messages are less than 65535 bytes
     * @param event Pointer to <code>pollfd</code> structure describing event
     * @param arrival_time Time of event arrival
     * @return <i>false</i> on success, <i>true</i> if error encountered and
     * handler was decomissioned
     */
    bool handle_event(struct pollfd *event,
                      ClockT::time_point arrival_time) override;

#if defined(__APPLE__) || defined(__FreeBSD__)
    /** Handle <code>kqueue()</code> interface events.
     * This method is called by its reactor thread to handle I/O events.
     * It handles <code>EVFILT_WRITE</code> events with a call to
     * #handle_write_readiness.  If #handle_write_readiness returns <i>true</i>
     * the handler is disconnected with a call to handle_disconnect() and
     * <i>true</i> is returned.  <code>EVFILT_READ</code> events are handled in
     * a loop by reading messages off the socket in their entirety with a call
     * to <code>recvfrom</code>.  The message buffer is 65536 bytes in size
     * and therefore this method can only handle messages of that size or
     * less.  Messages are delivered to the application by creating an Event
     * object, initializing it with the message header and
     * <code>arrival_time</code>, and delivering it with the default dispatch
     * handler.  Errors are handled by decomissioning
     * the handler and delivering an Event::ERROR event to the application.
     * @warning This method assumes messages are less than 65535 bytes
     * @param event Pointer to <code>kevent</code> structure describing event
     * @param arrival_time Time of event arrival
     * @return <i>false</i> on success, <i>true</i> if error encountered and
     * handler was decomissioned
     */
    bool handle_event(struct kevent *event,
                      ClockT::time_point arrival_time) override;
#elif defined(__linux__)
    /** Handle <code>epoll()</code> interface events.
     * This method is called by its reactor thread to handle I/O events.
     * It handles <code>EPOLLOUT</code> events with a call to
     * #handle_write_readiness.  If #handle_write_readiness returns <i>true</i>
     * the handler is disconnected with a call to handle_disconnect() and
     * <i>true</i> is returned.  <code>EPOLLIN</code> events are handled in
     * a loop by reading messages off the socket in their entirety with a call
     * to <code>recvfrom</code>.  The message buffer is 65536 bytes in size
     * and therefore this method can only handle messages of that size or
     * less.  Messages are delivered to the application by creating an Event
     * object, initializing it with the message header and
     * <code>arrival_time</code>, and delivering it with the default dispatch
     * handler.  <code>EPOLLERR</code> and errors are handled by decomissioning
     * the handler and delivering an Event::ERROR event to the application.
     * @warning This method assumes messages are less than 65535 bytes
     * @param event Pointer to <code>pollfd</code> structure describing event
     * @param arrival_time Time of event arrival
     * @return <i>false</i> on success, <i>true</i> if error encountered and
     * handler was decomissioned
     */
    bool handle_event(struct epoll_event *event,
                      ClockT::time_point arrival_time) override;
#elif defined(__sun__)
    /** Handle <code>port_associate()</code> interface events.
     * This method is called by its reactor thread to handle I/O events.
     * It handles <code>POLLOUT</code> events with a call to
     * #handle_write_readiness.  If #handle_write_readiness returns <i>true</i>
     * the handler is disconnected with a call to handle_disconnect() and
     * <i>true</i> is returned.  <code>POLLIN</code> events are handled in
     * a loop by reading messages off the socket in their entirety with a call
     * to <code>recvfrom</code>.  The message buffer is 65536 bytes in size
     * and therefore this method can only handle messages of that size or
     * less.  Messages are delivered to the application by creating an Event
     * object, initializing it with the message header and
     * <code>arrival_time</code>, and delivering it with the default dispatch
     * handler.  <code>POLLERR</code>, <code>POLLREMOVE</code>, and errors are
     * handled by decomissioning the handler and delivering an Event::ERROR
     * event to the application.
     * @warning This method assumes messages are less than 65535 bytes
     * @param event Pointer to <code>port_event_t</code> structure describing
     * event
     * @param arrival_time Time of event arrival
     * @return <i>false</i> on success, <i>true</i> if error encountered and
     * handler was decomissioned
     */
    bool handle_event(port_event_t *event,
                      ClockT::time_point arrival_time) override;
#else
    ImplementMe;
#endif

    /** Handles write readiness.  This method handles write readiness by
     * flushing the send queue with a call to #flush_send_queue and then
     * removes write interest from the polling interface for this handler
     * if the send queue becomes empty.
     * @return Error::OK on success, or Error::COMM_SEND_ERROR on send error
     */
    int handle_write_readiness();

  private:

    /// Send queue message record
    typedef std::pair<struct sockaddr_in, CommBufPtr> SendRec;

    /// Message receive buffer
    uint8_t *m_message;

    /// Send queue
    std::list<SendRec> m_send_queue;
  };
  /** @}*/
}

#endif // AsyncComm_IOHandlerDatagram_h
