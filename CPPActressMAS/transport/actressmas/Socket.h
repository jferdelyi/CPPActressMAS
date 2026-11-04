/**************************************************************************
 *                                                                        *
 *  Description: CPPActressMas multi-agent framework                      *
 *  Website:     https://github.com/jferdelyi/CPPActressMAS               *
 *  Copyright:   (c) 2023-Today, Jean-François Erdelyi                    *
 *                                                                        *
 *  CPP version of ActressMAS by Florin Leon                              *
 *  https://github.com/florinleon/ActressMas                              *
 *                                                                        *
 *  This program and the accompanying materials are made available under  *
 *  the terms of the Eclipse Public License 2.0, which is available at    *
 *  https://www.eclipse.org/legal/epl-2.0/ and in the LICENSE file.       *
 *                                                                        *
 *  SPDX-License-Identifier: EPL-2.0                                      *
 *                                                                        *
 **************************************************************************/

#pragma once

#include <chrono>
#include <cstdint>
#include <span>
#include <string>

#include "../Types.h"

/**
 * CPPActressMAS transport layer.
 **/
namespace cam::transport {

	/**
	 * A blocking TCP connection to a server, on POSIX sockets or Winsock. One thread reads; the writers hold their own
	 * lock. shutdown() wakes a reader blocked in receive(), close() releases the socket once that reader is gone.
	 **/
	class TcpConnection final {
	private:
		/**
		 * Native socket (a SOCKET on Windows, a descriptor elsewhere), INVALID when closed.
		 **/
		std::intptr_t m_socket;

	public:
		/**
		 * Native value of a closed socket.
		 **/
		static constexpr std::intptr_t INVALID = -1;

		/**
		 * Create a closed connection.
		 **/
		TcpConnection();

		/**
		 * Close the socket if it is still open.
		 **/
		~TcpConnection();

		// Delete copy constructor
		TcpConnection(const TcpConnection&) = delete;

		TcpConnection& operator=(const TcpConnection&) = delete;

		/**
		 * Connect to a server, bounded in time, and turn Nagle off: the frames are small and leave at once.
		 * @param p_host Host name or address
		 * @param p_port TCP port
		 * @param p_timeout Maximum time of the attempt
		 * @return Success, Timeout, or the error of the system
		 **/
		Result<void> connect(const std::string& p_host, int p_port, std::chrono::milliseconds p_timeout);

		/**
		 * Write every byte.
		 * @param p_bytes The bytes
		 * @return Success, or NotConnected once the connection is lost
		 **/
		Result<void> send_all(std::span<const std::uint8_t> p_bytes);

		/**
		 * Read exactly as many bytes as the buffer holds, blocking.
		 * @param p_buffer The buffer to fill
		 * @return False if the connection closed or failed first
		 **/
		bool receive(std::span<std::uint8_t> p_buffer);

		/**
		 * Stop both directions: a reader blocked in receive() returns false. The socket stays allocated.
		 **/
		void shutdown();

		/**
		 * Release the socket. No thread may use it any more.
		 **/
		void close();

		/**
		 * True between connect() and close().
		 * @return True if open
		 **/
		[[nodiscard]] bool is_open() const { return m_socket != INVALID; }
	};

} // namespace cam::transport
