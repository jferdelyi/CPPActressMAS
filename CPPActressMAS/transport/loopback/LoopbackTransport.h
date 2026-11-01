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

#include <functional>
#include <map>
#include <mutex>

#include "../Endpoint.h"

/**
 * CPPActressMAS transport layer.
 **/
namespace cam::transport {

	/**
	 * Options of the loopback transport. The serializing mode encodes and decodes every envelope with the
	 * codec, as a network transport would; the injections serve the tests of the end-to-end layer.
	 **/
	struct LoopbackOptions {
		/**
		 * True if every envelope is encoded and decoded with the codec, as a network transport does.
		 **/
		bool m_serialize = false;

		/**
		 * Drop one publication out of n (0: never).
		 **/
		std::size_t m_drop_every_n = 0;

		/**
		 * Deliver one publication out of n twice (0: never).
		 **/
		std::size_t m_duplicate_every_n = 0;

		/**
		 * Hold n publications then deliver them in reverse order (0: never).
		 **/
		std::size_t m_reorder_window = 0;

		/**
		 * Drop the publications matching the predicate.
		 **/
		std::function<bool(const Envelope&)> m_drop_if{};

		/**
		 * Hold the matching publications until release_held().
		 **/
		std::function<bool(const Envelope&)> m_hold_if{};
	};

	/**
	 * Transport for the environments of one process: direct delivery between endpoints, no socket.
	 * In-memory by default (no encoding), serializing on request. Presence is retained: a newcomer
	 * receives the last presence of every attached endpoint.
	 **/
	class LoopbackTransport final : public Transport {
	private:
		/**
		 * Configuration.
		 **/
		TransportConfig m_config;

		/**
		 * Options.
		 **/
		LoopbackOptions m_options;

		/**
		 * Codec of the serializing mode.
		 **/
		std::shared_ptr<const EnvelopeCodec> m_codec;

		/**
		 * Id of this node.
		 **/
		std::string m_node_id;

		/**
		 * True between open() and close().
		 **/
		bool m_open = false;

		/**
		 * Attached endpoints.
		 **/
		EndpointRegistry m_registry;

		/**
		 * Guards the counters, the retained presences and the held publications.
		 **/
		mutable std::mutex m_mutex;

		/**
		 * Publications received, dropped ones included.
		 **/
		std::size_t m_published = 0;

		/**
		 * Publications dropped by the injections.
		 **/
		std::size_t m_dropped = 0;

		/**
		 * Last presence of each online environment, by environment id.
		 **/
		std::map<std::string, Envelope> m_retained_presence;

		/**
		 * Publications held by the reordering window.
		 **/
		std::vector<std::pair<Address, Envelope>> m_reorder_buffer;

		/**
		 * Publications held by the hold predicate.
		 **/
		std::vector<std::pair<Address, Envelope>> m_held;

		/**
		 * Hand a publication to the attached endpoints, encoded in the serializing mode.
		 * @param p_sender The publishing endpoint (null for a released publication)
		 * @param p_address The destination
		 * @param p_envelope The envelope
		 **/
		void distribute(const Endpoint* p_sender, const Address& p_address, const Envelope& p_envelope);

	public:
		/**
		 * Create a loopback transport.
		 * @param p_config The configuration (namespace, maximum envelope size)
		 * @param p_options The options
		 * @param p_codec The codec of the serializing mode (protobuf by default)
		 **/
		explicit LoopbackTransport(TransportConfig p_config = {}, LoopbackOptions p_options = {},
								   std::shared_ptr<const EnvelopeCodec> p_codec = nullptr);

		/**
		 * Check the namespace; nothing to connect.
		 * @return Success, or InvalidConfiguration
		 **/
		Result<void> open() override;

		/**
		 * Stop accepting attachments and publications; nothing to wait for.
		 * @param p_timeout Unused
		 * @return Success
		 **/
		Result<void> close(std::chrono::milliseconds p_timeout) override;

		/**
		 * Attach an endpoint and deliver the retained presences to it.
		 * @param p_endpoint The endpoint
		 * @return Success, NotConnected or AlreadyConnected
		 **/
		Result<void> attach(Endpoint& p_endpoint) override;

		/**
		 * Detach an endpoint and forget its retained presence.
		 * @param p_endpoint The endpoint
		 * @return Success or NotAttached
		 **/
		Result<void> detach(Endpoint& p_endpoint) override;

		/**
		 * Deliver a publication to the attached endpoints, after the injections of the options.
		 * @param p_sender The publishing endpoint
		 * @param p_address The destination
		 * @param p_envelope The envelope
		 * @return Success, NotConnected or InvalidTarget
		 **/
		Result<void> publish(const Endpoint& p_sender, const Address& p_address, const Envelope& p_envelope) override;

		/**
		 * Capabilities: retained presence, and the ordering and delivery the options leave.
		 * @return The capabilities
		 **/
		[[nodiscard]] Capabilities capabilities() const override;

		/**
		 * Configuration.
		 * @return The configuration
		 **/
		[[nodiscard]] const TransportConfig& config() const override { return m_config; }

		/**
		 * Id of this node.
		 * @return The node id
		 **/
		[[nodiscard]] const std::string& node_id() const override { return m_node_id; }

		/**
		 * Codec of the serializing mode.
		 * @return The codec
		 **/
		[[nodiscard]] std::shared_ptr<const EnvelopeCodec> codec() const override { return m_codec; }

		/**
		 * Name of the transport.
		 * @return "loopback"
		 **/
		[[nodiscard]] const char* name() const override { return "loopback"; }

		/**
		 * Release the publications held by the hold predicate.
		 * @return How many were released
		 **/
		std::size_t release_held();

		/**
		 * Number of publications received by the transport (dropped ones included).
		 * @return The count
		 **/
		[[nodiscard]] std::size_t published() const;

		/**
		 * Number of publications dropped by the injections.
		 * @return The count
		 **/
		[[nodiscard]] std::size_t dropped() const;
	};

} // namespace cam::transport
