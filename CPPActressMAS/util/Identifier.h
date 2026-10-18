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

#include <cstddef>
#include <string>

/**
 * CPPActressMAS: identifiers of environments, nodes, transfers and agents.
 **/
namespace cam {

	/**
	 * Length of a generated agent identifier, the capacity of cam::AgentId.
	 **/
	constexpr std::size_t AGENT_ID_LENGTH = 15;

	/**
	 * Generate a random UUID of version 4 (RFC 9562): 36 lowercase hexadecimal digits and dashes. Used for
	 * environments, nodes and transfers. Thread-safe: one generator per thread, seeded once.
	 * @return The UUID
	 **/
	[[nodiscard]] std::string generate_uuid();

	/**
	 * Generate the identifier of an agent: AGENT_ID_LENGTH base62 characters, about 89 random bits.
	 * Thread-safe: one generator per thread, seeded once.
	 * @return The identifier
	 **/
	[[nodiscard]] std::string generate_agent_id();

} // namespace cam
