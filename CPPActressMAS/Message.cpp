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

#include "Message.h"

#include "util/Identifier.h"

namespace {

	const std::string s_no_text;
	const json s_no_json;
	const std::vector<std::uint8_t> s_no_bytes;

} // namespace

cam::Message::Message(std::string p_sender, std::string p_receiver, const json& p_message) :
		m_sender(std::move(p_sender)),
		m_receiver(std::move(p_receiver)),
		m_content(std::make_shared<const json>(p_message)) {
}

cam::Message::Message(std::string p_sender, std::string p_receiver, std::shared_ptr<const json> p_message) :
		m_sender(std::move(p_sender)),
		m_receiver(std::move(p_receiver)),
		m_content(p_message ? MessageContent(std::move(p_message)) : MessageContent()) {
}

cam::Message::Message(std::string p_sender, std::string p_receiver, std::string p_text) :
		m_sender(std::move(p_sender)),
		m_receiver(std::move(p_receiver)),
		m_content(std::move(p_text)) {
}

cam::Message::Message(std::string p_sender, std::string p_receiver, const uint8_t* p_message, const size_t& p_length) :
		m_sender(std::move(p_sender)),
		m_receiver(std::move(p_receiver)),
		m_content(p_message ? std::vector(p_message, p_message + p_length) : std::vector<std::uint8_t>()) {
}

cam::Message::Message(std::string p_sender, std::string p_receiver, MessageContent p_content) :
		m_sender(std::move(p_sender)),
		m_receiver(std::move(p_receiver)),
		m_content(std::move(p_content)) {
}

cam::Message::Message(Key, const std::string& p_sender, const std::string& p_receiver, MessageContent&& p_content,
					  const std::string& p_sender_environment) :
		// An agent identifier has a known length: copied with that constant length, the copy is inlined (P8)
		m_sender(p_sender.size() == AGENT_ID_LENGTH ? std::string(p_sender.data(), AGENT_ID_LENGTH) : p_sender),
		m_sender_environment(p_sender_environment),
		m_receiver(p_receiver.size() == AGENT_ID_LENGTH ? std::string(p_receiver.data(), AGENT_ID_LENGTH) : p_receiver),
		m_content(std::move(p_content)) {
}

cam::Message::Message() :
		m_sender(),
		m_receiver(),
		m_content() {
}

cam::ContentKind cam::Message::kind() const {
	switch (m_content.index()) {
		case 1: return ContentKind::Text;
		case 2: return ContentKind::Json;
		case 3: return ContentKind::Bytes;
		default: return ContentKind::None;
	}
}

const std::string& cam::Message::as_text() const {
	const auto* l_text = std::get_if<std::string>(&m_content);
	return l_text ? *l_text : s_no_text;
}

const json& cam::Message::as_json() const {
	const auto* l_json = std::get_if<std::shared_ptr<const json>>(&m_content);
	return l_json && *l_json ? **l_json : s_no_json;
}

const std::vector<std::uint8_t>& cam::Message::as_bytes() const {
	const auto* l_bytes = std::get_if<std::vector<std::uint8_t>>(&m_content);
	return l_bytes ? *l_bytes : s_no_bytes;
}

json cam::Message::content() const {
	switch (kind()) {
		case ContentKind::Text: return as_text();
		case ContentKind::Json: return as_json();
		case ContentKind::Bytes: return std::string(as_bytes().begin(), as_bytes().end());
		case ContentKind::None:
		default: return nullptr;
	}
}

std::string cam::Message::to_string() const {
	switch (kind()) {
		case ContentKind::Text: return as_text();
		case ContentKind::Json: return as_json().dump();
		case ContentKind::Bytes: return std::string(as_bytes().begin(), as_bytes().end());
		case ContentKind::None:
		default: return {};
	}
}

std::string cam::Message::format() const {
	return "[" + m_sender + " -> " + m_receiver + "]: " + to_string();
}
