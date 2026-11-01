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

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "../Endpoint.h"
#include "../network/PresenceCache.h"
#include "../network/Worker.h"

/**
 * CPPActressMAS transport layer: Zenoh (zenoh-c and zenoh-cpp 1.7). This header has no Zenoh include:
 * the session lives behind a private implementation.
 */
namespace cam::transport {

	/**
	 * Options of the Zenoh transport. Without multicast scouting, the session only reaches the
	 * endpoints given here (an environment stays silent by default).
	 **/
	struct ZenohOptions {
		std::string m_mode = "peer";             ///< "peer" or "client"
		std::vector<std::string> m_connect;      ///< endpoints to connect to, "tcp/127.0.0.1:7447"
		std::vector<std::string> m_listen;       ///< endpoints to listen on, "tcp/127.0.0.1:7447"
		bool m_multicast_scouting = false;       ///< discover peers by UDP multicast (asks the local network)
	};

	/**
	 * One Zenoh session per instance (one node). Publications go out reliably (congestion control
	 * "block"), the local endpoints are served directly and never through the network
	 * (allowed origin "remote"). Presence relies on liveliness tokens: one per node, one per
	 * environment; a newcomer learns the tokens with their history, then asks each environment for its
	 * current presence through a queryable. The death of a node drops its tokens: its environments go
	 * offline for everyone.
	 **/
	class ZenohTransport final : public Transport {
	private:
		struct Node;

		TransportConfig m_config;
		ZenohOptions m_options;
		std::shared_ptr<const EnvelopeCodec> m_codec;
		std::string m_node_id;
		std::unique_ptr<Node> m_node;
		EndpointRegistry m_registry;
		PresenceCache m_presence;
		Worker m_worker;
		std::atomic<bool> m_open{false};
		std::atomic<int> m_callbacks{0};  ///< Zenoh callbacks in flight: close() waits for them
		std::atomic<std::uint64_t> m_oversized{0};

		/**
		 * Counts a callback in flight: undeclaring or closing does not wait for a callback already running.
		 * Its constructor also marks, for ThreadSanitizer, the hand-over of the closure by the declaring thread.
		 **/
		struct CallbackGuard final {
			std::atomic<int>& m_counter;
			explicit CallbackGuard(std::atomic<int>& p_counter);
			~CallbackGuard() { --m_counter; }
			CallbackGuard(const CallbackGuard&) = delete;
			CallbackGuard& operator=(const CallbackGuard&) = delete;
		};

		[[nodiscard]] std::string key_of(AddressKind p_kind, const std::string& p_target = {}) const;
		[[nodiscard]] std::string live_prefix() const;
		void on_sample(const std::string& p_key, std::vector<std::uint8_t>&& p_bytes);
		void on_liveliness(const std::string& p_key, bool p_alive);
		void on_presence_reply(const std::string& p_environment_id, std::vector<std::uint8_t>&& p_bytes);
		void deliver_presence(const std::string& p_environment_id, Envelope&& p_envelope);
		void request_presence(const std::string& p_environment_id);
		void wait_callbacks();
		Result<void> publish_bytes(const std::string& p_key, std::vector<std::uint8_t>&& p_bytes);

	public:
		/**
		 * Create a Zenoh transport; nothing is opened yet.
		 * @param p_config The configuration (namespace, maximum envelope size)
		 * @param p_options Mode and endpoints
		 * @param p_codec The envelope codec (protobuf by default)
		 **/
		explicit ZenohTransport(TransportConfig p_config, ZenohOptions p_options = {},
								std::shared_ptr<const EnvelopeCodec> p_codec = nullptr);

		~ZenohTransport() override;

		Result<void> open() override;
		Result<void> close(std::chrono::milliseconds p_timeout) override;
		Result<void> attach(Endpoint& p_endpoint) override;
		Result<void> detach(Endpoint& p_endpoint) override;
		Result<void> publish(const Endpoint& p_sender, const Address& p_address, const Envelope& p_envelope) override;
		[[nodiscard]] Capabilities capabilities() const override;
		[[nodiscard]] const TransportConfig& config() const override { return m_config; }
		[[nodiscard]] const std::string& node_id() const override { return m_node_id; }
		[[nodiscard]] std::shared_ptr<const EnvelopeCodec> codec() const override { return m_codec; }
		[[nodiscard]] const char* name() const override { return "zenoh"; }

		/**
		 * Options of this instance.
		 * @return The options
		 **/
		[[nodiscard]] const ZenohOptions& options() const { return m_options; }

		/**
		 * Zenoh identifier of the session (hexadecimal), empty while closed.
		 * @return The identifier
		 **/
		[[nodiscard]] std::string session_id() const;

		/**
		 * Payloads dropped because larger than the configured maximum.
		 * @return The count
		 **/
		[[nodiscard]] std::uint64_t oversized() const { return m_oversized.load(); }
	};

} // namespace cam::transport
