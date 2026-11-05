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


#include "AggregateException.h"

#include <string>

namespace {

	// "One or more errors occurred ... (n): <message of the first one>", as the message of the C#
	std::string describe(const std::vector<std::exception_ptr>& p_exceptions) {
		std::string l_first = "unknown exception";
		if (!p_exceptions.empty()) {
			try {
				std::rethrow_exception(p_exceptions.front());
			} catch (const std::exception& p_exception) {
				l_first = p_exception.what();
			} catch (...) {
				// Not a std::exception: the default text stays
			}
		}
		return "One or more errors occurred in the agents of a Parallel turn (" + std::to_string(p_exceptions.size()) + "): " + l_first;
	}

} // namespace

cam::AggregateException::AggregateException(std::vector<std::exception_ptr> p_exceptions) :
		std::runtime_error(describe(p_exceptions)),
		m_exceptions(std::move(p_exceptions)) {}
