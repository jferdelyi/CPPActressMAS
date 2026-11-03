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

#include "Socket.h"

#include <cerrno>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace cam::transport {

	namespace {

#if defined(_WIN32)
		using NativeSocket = SOCKET;
		constexpr NativeSocket NO_SOCKET = INVALID_SOCKET;
#else
		using NativeSocket = int;
		constexpr NativeSocket NO_SOCKET = -1;
#endif

		NativeSocket native(const std::intptr_t p_socket) {
			return static_cast<NativeSocket>(p_socket);
		}

		// Winsock starts once per process and stays up to its end
		bool start_sockets() {
#if defined(_WIN32)
			static const bool s_started = [] {
				WSADATA l_data;
				return WSAStartup(MAKEWORD(2, 2), &l_data) == 0;
			}();
			return s_started;
#else
			return true;
#endif
		}

		void close_native(const NativeSocket p_socket) {
#if defined(_WIN32)
			closesocket(p_socket);
#else
			::close(p_socket);
#endif
		}

		void set_blocking(const NativeSocket p_socket, const bool p_blocking) {
#if defined(_WIN32)
			u_long l_non_blocking = p_blocking ? 0 : 1;
			ioctlsocket(p_socket, FIONBIO, &l_non_blocking);
#else
			const int l_flags = fcntl(p_socket, F_GETFL, 0);
			fcntl(p_socket, F_SETFL, p_blocking ? (l_flags & ~O_NONBLOCK) : (l_flags | O_NONBLOCK));
#endif
		}

		// True if a call failed only because a signal interrupted it
		bool interrupted() {
#if defined(_WIN32)
			return false;
#else
			return errno == EINTR;
#endif
		}

		Error socket_error(const std::string& p_message) {
			return make_error(ErrorCategory::Transport, ErrorCode::TransportFailure, p_message);
		}

	} // namespace

	TcpConnection::TcpConnection() :
			m_socket(INVALID) {
	}

	TcpConnection::~TcpConnection() {
		close();
	}

	Result<void> TcpConnection::connect(const std::string& p_host, const int p_port, const std::chrono::milliseconds p_timeout) {
		if (is_open()) {
			return make_error(ErrorCategory::State, ErrorCode::AlreadyConnected, "connection already open");
		}
		if (!start_sockets()) {
			return socket_error("the sockets of the system could not start");
		}
		addrinfo l_hints{};
		l_hints.ai_family = AF_UNSPEC;
		l_hints.ai_socktype = SOCK_STREAM;
		addrinfo* l_addresses = nullptr;
		const std::string l_port = std::to_string(p_port);
		if (getaddrinfo(p_host.c_str(), l_port.c_str(), &l_hints, &l_addresses) != 0 || !l_addresses) {
			return socket_error("unknown host " + p_host);
		}
		bool l_timed_out = false;
		for (const addrinfo* l_address = l_addresses; l_address && !is_open(); l_address = l_address->ai_next) {
			const NativeSocket l_socket = socket(l_address->ai_family, l_address->ai_socktype, l_address->ai_protocol);
			if (l_socket == NO_SOCKET) {
				continue;
			}
			// A non-blocking connection bounded by select, then the socket blocks again
			set_blocking(l_socket, false);
			bool l_connected = ::connect(l_socket, l_address->ai_addr, static_cast<int>(l_address->ai_addrlen)) == 0;
			if (!l_connected) {
				fd_set l_write_set;
				FD_ZERO(&l_write_set);
				FD_SET(l_socket, &l_write_set);
				timeval l_timeout{};
				l_timeout.tv_sec = static_cast<long>(p_timeout.count() / 1000);
				l_timeout.tv_usec = static_cast<long>((p_timeout.count() % 1000) * 1000);
				const int l_ready = select(static_cast<int>(l_socket) + 1, nullptr, &l_write_set, nullptr, &l_timeout);
				if (l_ready == 1) {
					int l_error = 0;
#if defined(_WIN32)
					int l_length = sizeof(l_error);
#else
					socklen_t l_length = sizeof(l_error);
#endif
					getsockopt(l_socket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&l_error), &l_length);
					l_connected = l_error == 0;
				} else if (l_ready == 0) {
					l_timed_out = true;
				}
			}
			if (!l_connected) {
				close_native(l_socket);
				continue;
			}
			set_blocking(l_socket, true);
			const int l_one = 1;
			setsockopt(l_socket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&l_one), sizeof(l_one));
#if defined(SO_NOSIGPIPE)
			// A write on a lost connection returns an error instead of raising SIGPIPE (macOS)
			setsockopt(l_socket, SOL_SOCKET, SO_NOSIGPIPE, reinterpret_cast<const char*>(&l_one), sizeof(l_one));
#endif
			m_socket = static_cast<std::intptr_t>(l_socket);
		}
		freeaddrinfo(l_addresses);
		if (!is_open()) {
			if (l_timed_out) {
				return make_error(ErrorCategory::Transport, ErrorCode::Timeout, "no answer from " + p_host + ":" + l_port);
			}
			return socket_error("connection to " + p_host + ":" + l_port + " refused");
		}
		return {};
	}

	Result<void> TcpConnection::send_all(const std::span<const std::uint8_t> p_bytes) {
		if (!is_open()) {
			return make_error(ErrorCategory::State, ErrorCode::NotConnected, "connection closed");
		}
		std::size_t l_sent = 0;
		while (l_sent < p_bytes.size()) {
#if defined(_WIN32)
			const int l_count = ::send(native(m_socket), reinterpret_cast<const char*>(p_bytes.data() + l_sent), static_cast<int>(p_bytes.size() - l_sent), 0);
#elif defined(MSG_NOSIGNAL)
			// A write on a lost connection returns an error instead of raising SIGPIPE (Linux)
			const auto l_count = ::send(native(m_socket), p_bytes.data() + l_sent, p_bytes.size() - l_sent, MSG_NOSIGNAL);
#else
			const auto l_count = ::send(native(m_socket), p_bytes.data() + l_sent, p_bytes.size() - l_sent, 0);
#endif
			if (l_count <= 0) {
				if (l_count < 0 && interrupted()) {
					continue;
				}
				return make_error(ErrorCategory::State, ErrorCode::NotConnected, "connection lost");
			}
			l_sent += static_cast<std::size_t>(l_count);
		}
		return {};
	}

	bool TcpConnection::receive(const std::span<std::uint8_t> p_buffer) {
		if (!is_open()) {
			return false;
		}
		std::size_t l_received = 0;
		while (l_received < p_buffer.size()) {
#if defined(_WIN32)
			const int l_count = ::recv(native(m_socket), reinterpret_cast<char*>(p_buffer.data() + l_received), static_cast<int>(p_buffer.size() - l_received), 0);
#else
			const auto l_count = ::recv(native(m_socket), p_buffer.data() + l_received, p_buffer.size() - l_received, 0);
#endif
			if (l_count <= 0) {
				if (l_count < 0 && interrupted()) {
					continue;
				}
				return false;
			}
			l_received += static_cast<std::size_t>(l_count);
		}
		return true;
	}

	void TcpConnection::shutdown() {
		if (!is_open()) {
			return;
		}
#if defined(_WIN32)
		::shutdown(native(m_socket), SD_BOTH);
#else
		::shutdown(native(m_socket), SHUT_RDWR);
#endif
	}

	void TcpConnection::close() {
		if (!is_open()) {
			return;
		}
		close_native(native(m_socket));
		m_socket = INVALID;
	}

} // namespace cam::transport
