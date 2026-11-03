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
#include <cstring>
#include <string>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace cam::test {

	/**
	 * Bounded TCP connection attempt. The network tests call it before opening an MQTT connection:
	 * without a broker, Environment::initialise_remote_connection() spins forever.
	 * @param p_host Host name or address
	 * @param p_port TCP port
	 * @param p_timeout Maximum time for the connection attempt
	 * @return True if a TCP connection could be established
	 **/
	inline bool tcp_port_open(const std::string& p_host, const int p_port, const std::chrono::milliseconds p_timeout) {
#if defined(_WIN32)
		WSADATA l_wsa_data;
		if (WSAStartup(MAKEWORD(2, 2), &l_wsa_data) != 0) {
			return false;
		}
#endif
		addrinfo l_hints{};
		l_hints.ai_family = AF_UNSPEC;
		l_hints.ai_socktype = SOCK_STREAM;
		addrinfo* l_result = nullptr;
		const std::string l_port = std::to_string(p_port);
		if (getaddrinfo(p_host.c_str(), l_port.c_str(), &l_hints, &l_result) != 0 || !l_result) {
#if defined(_WIN32)
			WSACleanup();
#endif
			return false;
		}

		bool l_open = false;
		for (const addrinfo* l_address = l_result; l_address && !l_open; l_address = l_address->ai_next) {
#if defined(_WIN32)
			const SOCKET l_socket = socket(l_address->ai_family, l_address->ai_socktype, l_address->ai_protocol);
			if (l_socket == INVALID_SOCKET) {
				continue;
			}
			u_long l_non_blocking = 1;
			ioctlsocket(l_socket, FIONBIO, &l_non_blocking);
#else
			const int l_socket = socket(l_address->ai_family, l_address->ai_socktype, l_address->ai_protocol);
			if (l_socket < 0) {
				continue;
			}
			fcntl(l_socket, F_SETFL, fcntl(l_socket, F_GETFL, 0) | O_NONBLOCK);
#endif
			const int l_rc = connect(l_socket, l_address->ai_addr, static_cast<int>(l_address->ai_addrlen));
			if (l_rc == 0) {
				l_open = true;
			} else {
				fd_set l_write_set;
				FD_ZERO(&l_write_set);
				FD_SET(l_socket, &l_write_set);
				timeval l_timeout{};
				l_timeout.tv_sec = static_cast<long>(p_timeout.count() / 1000);
				l_timeout.tv_usec = static_cast<long>((p_timeout.count() % 1000) * 1000);
				if (select(static_cast<int>(l_socket) + 1, nullptr, &l_write_set, nullptr, &l_timeout) == 1) {
					int l_error = 0;
#if defined(_WIN32)
					int l_length = sizeof(l_error);
					getsockopt(l_socket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&l_error), &l_length);
#else
					socklen_t l_length = sizeof(l_error);
					getsockopt(l_socket, SOL_SOCKET, SO_ERROR, &l_error, &l_length);
#endif
					l_open = l_error == 0;
				}
			}
#if defined(_WIN32)
			closesocket(l_socket);
#else
			close(l_socket);
#endif
		}
		freeaddrinfo(l_result);
#if defined(_WIN32)
		WSACleanup();
#endif
		return l_open;
	}

} // namespace cam::test
