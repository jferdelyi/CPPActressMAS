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

#include <exception>
#include <stdexcept>
#include <vector>

/**
 * CPPActressMAS
 **/
namespace cam {

	/**
	 * The exceptions thrown by the agents of a Parallel turn, as the AggregateException of Parallel.Invoke in the C#:
	 * every agent of the turn runs to its end, then start() or continue_simulation() throws them together, in the order
	 * of the agents in the turn.
	 **/
	class AggregateException final : public std::runtime_error {
	private:
		/**
		 * The exceptions, in the order of the agents in the turn.
		 **/
		std::vector<std::exception_ptr> m_exceptions;

	public:
		/**
		 * Gather the exceptions of a turn.
		 * @param p_exceptions The exceptions, in the order of the agents in the turn
		 **/
		explicit AggregateException(std::vector<std::exception_ptr> p_exceptions);

		/**
		 * The exceptions of the turn, each one to be rethrown by std::rethrow_exception.
		 * @return The exceptions, in the order of the agents in the turn
		 **/
		[[nodiscard]] const std::vector<std::exception_ptr>& exceptions() const { return m_exceptions; }
	};

} // namespace cam
