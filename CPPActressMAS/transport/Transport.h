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
#include <memory>
#include <shared_mutex>
#include <string>
#include <vector>

#include "Envelope.h"
#include "codec/EnvelopeCodec.h"

/**
 * CPPActressMAS transport layer: the interface every transport implements. The core never includes a
 * network header; transports are separate libraries behind CMake options.
 */
namespace cam::transport {

	class Endpoint;

	/**
	 * A transport instance is a node: one connection, one namespace, one codec, one node id. It may serve
	 * every environment of a process (one Endpoint each). Publications of an attached endpoint are first
	 * delivered to the other attached endpoints that subscribe to the key (local delivery never goes
	 * through the network), then published once on the network when the key requires it.
	 **/
	class Transport {
	public:
		virtual ~Transport() = default;

		/**
		 * Open the transport (bounded by the configured connection timeout, never a busy wait).
		 * @return Success or an error
		 **/
		virtual Result<void> open() = 0;

		/**
		 * Close the transport, bounded in time.
		 * @param p_timeout Maximum time
		 * @return Success or an error
		 **/
		virtual Result<void> close(std::chrono::milliseconds p_timeout) = 0;

		/**
		 * Attach an endpoint: subscribe its keys, deliver the known presences to it.
		 * @param p_endpoint The endpoint
		 * @return Success or an error
		 **/
		virtual Result<void> attach(Endpoint& p_endpoint) = 0;

		/**
		 * Detach an endpoint.
		 * @param p_endpoint The endpoint
		 * @return Success or an error
		 **/
		virtual Result<void> detach(Endpoint& p_endpoint) = 0;

		/**
		 * Publish an envelope to an address, locally and on the network as the key requires. The transport
		 * encodes with its codec when it needs bytes. Called by the simulation thread of the sending
		 * endpoint (outbox flush).
		 * @param p_sender The publishing endpoint
		 * @param p_address The destination
		 * @param p_envelope The envelope
		 * @return Success or an error
		 **/
		virtual Result<void> publish(const Endpoint& p_sender, const Address& p_address, const Envelope& p_envelope) = 0;

		/**
		 * What this transport can do.
		 * @return The capabilities
		 **/
		[[nodiscard]] virtual Capabilities capabilities() const = 0;

		/**
		 * Configuration of this instance.
		 * @return The configuration
		 **/
		[[nodiscard]] virtual const TransportConfig& config() const = 0;

		/**
		 * Identifier of this node (drawn at construction).
		 * @return The node id
		 **/
		[[nodiscard]] virtual const std::string& node_id() const = 0;

		/**
		 * Codec of this instance.
		 * @return The codec
		 **/
		[[nodiscard]] virtual std::shared_ptr<const EnvelopeCodec> codec() const = 0;

		/**
		 * Short name of the transport (configuration and logs).
		 * @return The name
		 **/
		[[nodiscard]] virtual const char* name() const = 0;
	};

	/**
	 * Registry of the endpoints attached to a transport, with the local fan-out shared by every
	 * transport. Thread-safe: transports call it from their network threads. Deliveries run under a
	 * shared lock and never hand a pointer out; remove() takes the exclusive lock, so once it returns
	 * no delivery to that endpoint is in flight and the endpoint may be destroyed.
	 **/
	class EndpointRegistry final {
	private:
		mutable std::shared_mutex m_mutex;
		std::vector<Endpoint*> m_endpoints;

		[[nodiscard]] Endpoint* find_unlocked(const std::string& p_environment_id) const;

	public:
		/**
		 * Register an endpoint.
		 * @param p_endpoint The endpoint
		 * @return False if it was already registered
		 **/
		bool add(Endpoint& p_endpoint);

		/**
		 * Unregister an endpoint, once the deliveries in flight are done.
		 * @param p_endpoint The endpoint
		 * @return False if it was not registered
		 **/
		bool remove(Endpoint& p_endpoint);

		/**
		 * True if an environment is attached here.
		 * @param p_environment_id The environment id
		 * @return True if local
		 **/
		[[nodiscard]] bool is_local(const std::string& p_environment_id) const;

		/**
		 * Run an action on the endpoint of an environment, under the shared lock.
		 * @param p_environment_id The environment id
		 * @param p_action Callable taking Endpoint&
		 * @return False if the environment is not attached here
		 **/
		template<typename Action>
		bool with(const std::string& p_environment_id, Action&& p_action) const {
			const std::shared_lock<std::shared_mutex> l_lock(m_mutex);
			if (Endpoint* l_endpoint = find_unlocked(p_environment_id); l_endpoint) {
				p_action(*l_endpoint);
				return true;
			}
			return false;
		}

		/**
		 * Run an action on every registered endpoint but one, under the shared lock.
		 * @param p_except Endpoint to leave out (may be null)
		 * @param p_action Callable taking Endpoint&
		 **/
		template<typename Action>
		void for_each(const Endpoint* p_except, Action&& p_action) const {
			const std::shared_lock<std::shared_mutex> l_lock(m_mutex);
			for (Endpoint* l_endpoint: m_endpoints) {
				if (l_endpoint != p_except) {
					p_action(*l_endpoint);
				}
			}
		}

		/**
		 * Result of a local fan-out.
		 **/
		struct LocalDelivery {
			bool m_delivered_locally = false;  ///< at least one local endpoint received it
			bool m_needs_network = false;      ///< the key also goes to the network
			bool m_target_is_local = false;    ///< unicast key whose environment is attached here
		};

		/**
		 * Deliver an envelope to the attached endpoints that subscribe to the address (never the sender)
		 * and say whether the network is needed. Unicast keys (EnvIn, EnvAgent, EnvQuery) of a local
		 * environment never go to the network; AllIn and presence keys go locally and to the network;
		 * NodePresence goes to the network only.
		 * @param p_sender The publishing endpoint (may be null for network input)
		 * @param p_address The destination
		 * @param p_envelope The envelope
		 * @return What happened
		 **/
		LocalDelivery deliver_locally(const Endpoint* p_sender, const Address& p_address, const Envelope& p_envelope) const;
	};

} // namespace cam::transport
