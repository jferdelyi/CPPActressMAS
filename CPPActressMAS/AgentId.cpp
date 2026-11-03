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

#include "AgentId.h"

#include <stdexcept>

#include <nlohmann/json.hpp>

#include "util/Identifier.h"

cam::AgentId::AgentId(const std::string_view p_id) {
	if (p_id.size() > CAPACITY) {
		throw std::invalid_argument("an agent identifier has at most 15 characters: " + std::string(p_id));
	}
	std::memcpy(m_chars.data(), p_id.data(), p_id.size());
	m_length = static_cast<std::uint8_t>(p_id.size());
}

std::optional<cam::AgentId> cam::AgentId::parse(const std::string_view p_id) noexcept {
	if (p_id.size() > CAPACITY) {
		return std::nullopt;
	}
	AgentId l_id;
	std::memcpy(l_id.m_chars.data(), p_id.data(), p_id.size());
	l_id.m_length = static_cast<std::uint8_t>(p_id.size());
	return l_id;
}

cam::AgentId cam::AgentId::generate() {
	return AgentId(generate_agent_id());
}

std::ostream& cam::operator<<(std::ostream& p_stream, const AgentId& p_id) {
	return p_stream << p_id.view();
}

void cam::to_json(nlohmann::json& p_json, const AgentId& p_id) {
	p_json = p_id.str();
}

void cam::from_json(const nlohmann::json& p_json, AgentId& p_id) {
	p_id = AgentId(p_json.get<std::string>());
}
