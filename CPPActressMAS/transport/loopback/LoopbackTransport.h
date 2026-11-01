/**************************************************************************
 *                                                                        *
 *  Description: CPPActressMas multi-agent framework                      *
 *  Website:     https://github.com/jferdelyi/CPPActressMAS               *
 *  Copyright:   (c) 2023-Today, Jean-François Erdelyi                    *
 *                                                                        *
 *  CPP version of ActressMAS by Florin Leon                              *
 *  https://github.com/florinleon/ActressMas                              *
 *                                                                        *
 *  This program is free software; you can redistribute it and/or modify  *
 *  it under the terms of the GNU General License as published by         *
 *  the Free Software Foundation. This program is distributed in the      *
 *  hope that it will be useful, but WITHOUT ANY WARRANTY; without even   *
 *  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR   *
 *  PURPOSE. See the GNU General License for more details.                *
 *                                                                        *
 **************************************************************************/

#pragma once

#include <functional>
#include <map>
#include <mutex>

#include "../Endpoint.h"

/**
 * CPPActressMAS transport layer.
 */
namespace cam::transport {

	/**
	 * Options of the loopback transport. The serializing mode encodes and decodes every envelope with the
	 * codec, as a network transport would; the injections serve the tests of the end-to-end layer.
	 **/
	struct LoopbackOptions {
		bool m_serialize = false;
		std::size_t m_drop_every_n = 0;       ///< drop one publication out of n (0: never)
		std::size_t m_duplicate_every_n = 0;  ///< deliver one publication out of n twice (0: never)
		std::size_t m_reorder_window = 0;     ///< hold n publications then deliver them in reverse order (0: never)
		std::function<bool(const Envelope&)> m_drop_if{};  ///< drop the publications matching the predicate
		std::function<bool(const Envelope&)> m_hold_if{};  ///< hold the matching publications until release_held()
	};

	/**
	 * Transport for the environments of one process: direct delivery between endpoints, no socket.
	 * In-memory by default (no encoding), serializing on request. Presence is retained: a newcomer
	 * receives the last presence of every attached endpoint.
	 **/
	class LoopbackTransport final : public Transport {
	private:
		TransportConfig m_config;
		LoopbackOptions m_options;
		std::shared_ptr<const EnvelopeCodec> m_codec;
		std::string m_node_id;
		bool m_open = false;
		EndpointRegistry m_registry;

		mutable std::mutex m_mutex;
		std::size_t m_published = 0;
		std::size_t m_dropped = 0;
		std::map<std::string, Envelope> m_retained_presence;  ///< environment id -> last presence
		std::vector<std::pair<Address, Envelope>> m_reorder_buffer;
		std::vector<std::pair<Address, Envelope>> m_held;

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

		Result<void> open() override;
		Result<void> close(std::chrono::milliseconds p_timeout) override;
		Result<void> attach(Endpoint& p_endpoint) override;
		Result<void> detach(Endpoint& p_endpoint) override;
		Result<void> publish(const Endpoint& p_sender, const Address& p_address, const Envelope& p_envelope) override;
		[[nodiscard]] Capabilities capabilities() const override;
		[[nodiscard]] const TransportConfig& config() const override { return m_config; }
		[[nodiscard]] const std::string& node_id() const override { return m_node_id; }
		[[nodiscard]] std::shared_ptr<const EnvelopeCodec> codec() const override { return m_codec; }
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
