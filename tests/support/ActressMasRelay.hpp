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

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <transport/actressmas/ActressMasCodec.h>

namespace cam::test {

	/**
	 * Relay of the tests, with the rules of the ContainerServer of ActressMAS v3.1 (Server.cs): it listens on 127.0.0.1,
	 * on a port of the system; a container registers under a name that no other one holds (a taken name gets "Inform
	 * Invalid Name", then its connection is closed); every period, each registered container gets the list of the
	 * registered names, each followed by a space; a "Send Remote Message" or a "Request Move Agent" of a registered
	 * container goes to the container named by its receiver, and nowhere when no container has that name. One thread
	 * accepts, one reads each container, one ticks. The C# server relays each message on a thread of its pool, so two
	 * messages of a container may go out in the other order; this relay keeps their order, unless it is asked to swap
	 * them (one more thread then sends the delayed ones).
	 **/
	class ActressMasRelay final {
	private:
#if defined(_WIN32)
		using Socket = SOCKET;
		static constexpr Socket NO_SOCKET = INVALID_SOCKET;
#else
		using Socket = int;
		static constexpr Socket NO_SOCKET = -1;
#endif

		// A connected container
		struct Client {
			Socket m_socket = NO_SOCKET;
			std::string m_name;  // empty until it registers, guarded by m_mutex
			std::size_t m_relayed = 0;  // its messages relayed so far, guarded by m_mutex
			std::mutex m_write_mutex;
			std::thread m_reader;
		};

		// A message that goes out later
		struct Delayed {
			std::chrono::steady_clock::time_point m_due;
			std::vector<std::shared_ptr<Client>> m_targets;
			cam::transport::ContainerMessage m_message;
		};

		std::chrono::milliseconds m_period;
		std::chrono::milliseconds m_swap;
		Socket m_listener = NO_SOCKET;
		int m_port = 0;
		std::atomic<bool> m_running{false};
		std::thread m_acceptor;
		std::thread m_ticker;
		std::thread m_delayer;
		std::mutex m_mutex;
		std::condition_variable m_condition;
		std::condition_variable m_delayed_condition;
		std::vector<std::shared_ptr<Client>> m_clients;
		std::vector<cam::transport::ContainerMessage> m_relayed;
		std::deque<Delayed> m_delayed;

		static void close_socket(const Socket p_socket) {
#if defined(_WIN32)
			closesocket(p_socket);
#else
			::close(p_socket);
#endif
		}

		static void shutdown_socket(const Socket p_socket) {
#if defined(_WIN32)
			::shutdown(p_socket, SD_BOTH);
#else
			::shutdown(p_socket, SHUT_RDWR);
#endif
		}

		static bool read_exact(const Socket p_socket, const std::span<std::uint8_t> p_buffer) {
			std::size_t l_received = 0;
			while (l_received < p_buffer.size()) {
#if defined(_WIN32)
				const int l_count = ::recv(p_socket, reinterpret_cast<char*>(p_buffer.data() + l_received), static_cast<int>(p_buffer.size() - l_received), 0);
#else
				const auto l_count = ::recv(p_socket, p_buffer.data() + l_received, p_buffer.size() - l_received, 0);
#endif
				if (l_count <= 0) {
					return false;
				}
				l_received += static_cast<std::size_t>(l_count);
			}
			return true;
		}

		static bool send_bytes(Client& p_client, const std::span<const std::uint8_t> p_bytes) {
			const std::lock_guard<std::mutex> l_lock(p_client.m_write_mutex);
			std::size_t l_sent = 0;
			while (l_sent < p_bytes.size()) {
#if defined(_WIN32)
				const int l_count = ::send(p_client.m_socket, reinterpret_cast<const char*>(p_bytes.data() + l_sent), static_cast<int>(p_bytes.size() - l_sent), 0);
#elif defined(MSG_NOSIGNAL)
				const auto l_count = ::send(p_client.m_socket, p_bytes.data() + l_sent, p_bytes.size() - l_sent, MSG_NOSIGNAL);
#else
				const auto l_count = ::send(p_client.m_socket, p_bytes.data() + l_sent, p_bytes.size() - l_sent, 0);
#endif
				if (l_count <= 0) {
					return false;
				}
				l_sent += static_cast<std::size_t>(l_count);
			}
			return true;
		}

		static bool send_message(Client& p_client, const cam::transport::ContainerMessage& p_message) {
			const auto l_bytes = cam::transport::ActressMasCodec::encode_frame(p_message);
			return send_bytes(p_client, l_bytes);
		}

		void accept_containers() {
			while (m_running.load()) {
				// select with a short timeout: the loop sees the stop without closing the listener under it
				fd_set l_set;
				FD_ZERO(&l_set);
				FD_SET(m_listener, &l_set);
				timeval l_timeout{};
				l_timeout.tv_usec = 20000;
				if (select(static_cast<int>(m_listener) + 1, &l_set, nullptr, nullptr, &l_timeout) <= 0) {
					continue;
				}
				const Socket l_socket = accept(m_listener, nullptr, nullptr);
				if (l_socket == NO_SOCKET) {
					continue;
				}
				const int l_one = 1;
				setsockopt(l_socket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&l_one), sizeof(l_one));
#if defined(SO_NOSIGPIPE)
				setsockopt(l_socket, SOL_SOCKET, SO_NOSIGPIPE, reinterpret_cast<const char*>(&l_one), sizeof(l_one));
#endif
				auto l_client = std::make_shared<Client>();
				l_client->m_socket = l_socket;
				const std::lock_guard<std::mutex> l_lock(m_mutex);
				m_clients.push_back(l_client);
				l_client->m_reader = std::thread(&ActressMasRelay::read_container, this, l_client);
			}
		}

		void read_container(const std::shared_ptr<Client> p_client) {
			using cam::transport::ActressMasCodec;
			std::vector<std::uint8_t> l_frame(ActressMasCodec::HEADER_SIZE);
			while (read_exact(p_client->m_socket, std::span<std::uint8_t>(l_frame.data(), ActressMasCodec::HEADER_SIZE))) {
				const auto l_size = ActressMasCodec::read_frame_size(l_frame, 1u << 20);
				if (!l_size) {
					break;
				}
				l_frame.resize(l_size.value());
				if (!read_exact(p_client->m_socket, std::span<std::uint8_t>(l_frame.data() + ActressMasCodec::HEADER_SIZE,
																			 l_size.value() - ActressMasCodec::HEADER_SIZE))) {
					break;
				}
				if (const auto l_message = ActressMasCodec::decode_frame(l_frame, 1u << 20); l_message) {
					process(*p_client, l_message.value());
				}
				l_frame.resize(ActressMasCodec::HEADER_SIZE);
			}
			// Gone: its name is free again
			const std::lock_guard<std::mutex> l_lock(m_mutex);
			p_client->m_name.clear();
		}

		void process(Client& p_client, const cam::transport::ContainerMessage& p_message) {
			using cam::transport::ActressMasCodec;
			if (p_message.m_sender == ActressMasCodec::SERVER) {
				return;
			}
			if (p_message.m_receiver == ActressMasCodec::SERVER && p_message.m_info == ActressMasCodec::REQUEST_REGISTER) {
				std::unique_lock<std::mutex> l_lock(m_mutex);
				const bool l_taken = std::any_of(m_clients.begin(), m_clients.end(), [&](const auto& p_other) {
					return p_other->m_name == p_message.m_sender;
				});
				if (!l_taken) {
					p_client.m_name = p_message.m_sender;
					return;
				}
				l_lock.unlock();
				send_message(p_client, {std::string(ActressMasCodec::SERVER), p_message.m_sender, std::string(ActressMasCodec::INFORM_INVALID_NAME), ""});
				shutdown_socket(p_client.m_socket);
				return;
			}
			if (p_message.m_info != ActressMasCodec::SEND_REMOTE_MESSAGE && p_message.m_info != ActressMasCodec::REQUEST_MOVE_AGENT) {
				return;
			}
			std::vector<std::shared_ptr<Client>> l_targets;
			{
				// Relayed for a registered sender only, as the server checks the name (not who writes it)
				const std::lock_guard<std::mutex> l_lock(m_mutex);
				const bool l_registered = std::any_of(m_clients.begin(), m_clients.end(), [&](const auto& p_other) {
					return p_other->m_name == p_message.m_sender;
				});
				if (!l_registered) {
					return;
				}
				for (const auto& l_other: m_clients) {
					if (l_other->m_name == p_message.m_receiver) {
						l_targets.push_back(l_other);
					}
				}
				m_relayed.push_back(p_message);
				// Swapped: the first of each two messages of a container goes out after the second
				if (m_swap.count() > 0 && p_client.m_relayed++ % 2 == 0) {
					m_delayed.push_back({std::chrono::steady_clock::now() + m_swap, std::move(l_targets), p_message});
					m_delayed_condition.notify_all();
					return;
				}
			}
			for (const auto& l_target: l_targets) {
				send_message(*l_target, p_message);
			}
		}

		void send_delayed() {
			std::unique_lock<std::mutex> l_lock(m_mutex);
			while (m_running.load()) {
				if (m_delayed.empty()) {
					m_delayed_condition.wait(l_lock);
					continue;
				}
				// Same delay for all: the first one is due first
				if (const auto l_due = m_delayed.front().m_due; std::chrono::steady_clock::now() < l_due) {
					m_delayed_condition.wait_until(l_lock, l_due);
					continue;
				}
				const Delayed l_delayed = std::move(m_delayed.front());
				m_delayed.pop_front();
				l_lock.unlock();
				for (const auto& l_target: l_delayed.m_targets) {
					send_message(*l_target, l_delayed.m_message);
				}
				l_lock.lock();
			}
		}

		void tick() {
			std::unique_lock<std::mutex> l_lock(m_mutex);
			while (m_running.load()) {
				m_condition.wait_for(l_lock, m_period);
				if (!m_running.load()) {
					break;
				}
				std::string l_list;
				std::vector<std::shared_ptr<Client>> l_registered;
				for (const auto& l_client: m_clients) {
					if (!l_client->m_name.empty()) {
						l_list += l_client->m_name + " ";
						l_registered.push_back(l_client);
					}
				}
				std::vector<std::string> l_names;
				for (const auto& l_client: l_registered) {
					l_names.push_back(l_client->m_name);
				}
				l_lock.unlock();
				for (std::size_t i = 0; i < l_registered.size(); i++) {
					send_message(*l_registered[i], {std::string(cam::transport::ActressMasCodec::SERVER), l_names[i],
													std::string(cam::transport::ActressMasCodec::INFORM_CONTAINERS), l_list});
				}
				l_lock.lock();
			}
		}

	public:
		/**
		 * Create a relay; nothing listens yet.
		 * @param p_period Period of the list of the containers (3 s in the C# examples, short for the tests)
		 * @param p_swap Delay of the first of each two messages of a container, which then goes out after the second
		 * (0: none, the order is kept)
		 **/
		explicit ActressMasRelay(const std::chrono::milliseconds p_period = std::chrono::milliseconds(50),
								 const std::chrono::milliseconds p_swap = std::chrono::milliseconds(0)) :
				m_period(p_period),
				m_swap(p_swap) {}

		/**
		 * Stop the relay.
		 **/
		~ActressMasRelay() { stop(); }

		// Delete copy constructor
		ActressMasRelay(const ActressMasRelay&) = delete;

		ActressMasRelay& operator=(const ActressMasRelay&) = delete;

		/**
		 * Listen on 127.0.0.1, on a port of the system, and start the threads.
		 * @return False if it cannot listen
		 **/
		bool start() {
#if defined(_WIN32)
			WSADATA l_data;
			if (WSAStartup(MAKEWORD(2, 2), &l_data) != 0) {
				return false;
			}
#endif
			m_listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (m_listener == NO_SOCKET) {
				return false;
			}
			sockaddr_in l_address{};
			l_address.sin_family = AF_INET;
			l_address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
			l_address.sin_port = 0;
#if defined(_WIN32)
			int l_length = sizeof(l_address);
#else
			socklen_t l_length = sizeof(l_address);
#endif
			if (bind(m_listener, reinterpret_cast<const sockaddr*>(&l_address), sizeof(l_address)) != 0 || listen(m_listener, 16) != 0 ||
				getsockname(m_listener, reinterpret_cast<sockaddr*>(&l_address), &l_length) != 0) {
				close_socket(m_listener);
				m_listener = NO_SOCKET;
				return false;
			}
			m_port = ntohs(l_address.sin_port);
			m_running.store(true);
			m_acceptor = std::thread(&ActressMasRelay::accept_containers, this);
			m_ticker = std::thread(&ActressMasRelay::tick, this);
			if (m_swap.count() > 0) {
				m_delayer = std::thread(&ActressMasRelay::send_delayed, this);
			}
			return true;
		}

		/**
		 * Close every connection and stop the threads (idempotent): the containers see the server go.
		 **/
		void stop() {
			if (!m_running.exchange(false)) {
				return;
			}
			m_condition.notify_all();
			m_acceptor.join();
			m_ticker.join();
			if (m_delayer.joinable()) {
				{
					// Under the lock: the delayer is either waiting or sees the stop before it waits
					const std::lock_guard<std::mutex> l_lock(m_mutex);
					m_delayed_condition.notify_all();
				}
				m_delayer.join();
			}
			close_socket(m_listener);
			m_listener = NO_SOCKET;
			std::vector<std::shared_ptr<Client>> l_clients;
			{
				const std::lock_guard<std::mutex> l_lock(m_mutex);
				l_clients = m_clients;
			}
			// Every reader wakes and ends before any socket is released: no number is reused under a writer
			for (const auto& l_client: l_clients) {
				shutdown_socket(l_client->m_socket);
			}
			for (const auto& l_client: l_clients) {
				if (l_client->m_reader.joinable()) {
					l_client->m_reader.join();
				}
			}
			for (const auto& l_client: l_clients) {
				close_socket(l_client->m_socket);
			}
		}

		/**
		 * Port the relay listens on.
		 * @return The port, 0 before start()
		 **/
		[[nodiscard]] int port() const { return m_port; }

		/**
		 * The messages relayed so far, in order.
		 * @return A copy of them
		 **/
		std::vector<cam::transport::ContainerMessage> relayed() {
			const std::lock_guard<std::mutex> l_lock(m_mutex);
			return m_relayed;
		}

		/**
		 * Write bytes as they are to a registered container (a frame the tests make up).
		 * @param p_container The name of the container
		 * @param p_bytes The bytes
		 * @return False if no container has that name
		 **/
		bool send_raw(const std::string& p_container, const std::span<const std::uint8_t> p_bytes) {
			std::shared_ptr<Client> l_target;
			{
				const std::lock_guard<std::mutex> l_lock(m_mutex);
				for (const auto& l_client: m_clients) {
					if (l_client->m_name == p_container) {
						l_target = l_client;
					}
				}
			}
			return l_target && send_bytes(*l_target, p_bytes);
		}
	};

} // namespace cam::test
