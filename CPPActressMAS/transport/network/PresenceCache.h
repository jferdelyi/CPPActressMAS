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

#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "../Endpoint.h"

/**
 * CPPActressMAS transport layer.
 */
namespace cam::transport {

	/**
	 * Current presence of each environment seen by a transport, local or from the network: the last
	 * presence published, kept up to date with the directory events that follow it, replayed to the
	 * endpoints attached later and served to the peers that ask for it. Thread-safe.
	 **/
	class PresenceCache final {
	private:
		mutable std::mutex m_mutex;
		std::map<std::string, Envelope> m_entries;  ///< environment id -> current presence envelope

	public:
		/**
		 * Take an envelope published on a presence key into account: a presence replaces the entry of its
		 * environment (an offline one removes it), a directory event updates the agents of the entries.
		 * @param p_address The key it travels on
		 * @param p_envelope The envelope, decoded
		 **/
		void remember(const Address& p_address, const Envelope& p_envelope);

		/**
		 * Forget an environment.
		 * @param p_environment_id The environment
		 **/
		void forget(const std::string& p_environment_id);

		/**
		 * The current presence of an environment.
		 * @param p_environment_id The environment
		 * @return A copy of the envelope, or nothing
		 **/
		[[nodiscard]] std::optional<Envelope> find(const std::string& p_environment_id) const;

		/**
		 * Deliver every remembered presence but its own to an endpoint.
		 * @param p_endpoint The endpoint
		 **/
		void replay(Endpoint& p_endpoint) const;
	};

} // namespace cam::transport
