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

#include <stdexcept>

#include "util/RecyclingAllocator.hpp"

namespace {

	// White space of Char.IsWhiteSpace: the separators of String.Split() without argument
	bool is_white_space(const char32_t p_code) {
		return (p_code >= 0x09 && p_code <= 0x0D) || p_code == 0x20 || p_code == 0x85 || p_code == 0xA0 || p_code == 0x1680 ||
			   (p_code >= 0x2000 && p_code <= 0x200A) || p_code == 0x2028 || p_code == 0x2029 || p_code == 0x202F ||
			   p_code == 0x205F || p_code == 0x3000;
	}

	// Words of a UTF-8 text split at every white space, empty words kept, as String.Split() (an invalid byte is one
	// character, never a space)
	std::vector<std::string> split_words(const std::string& p_text) {
		std::vector<std::string> l_words(1);
		for (std::size_t i = 0; i < p_text.size();) {
			const auto l_lead = static_cast<unsigned char>(p_text[i]);
			std::size_t l_length = l_lead < 0x80 ? 1 : (l_lead >> 5) == 0x6 ? 2 : (l_lead >> 4) == 0xE ? 3 : (l_lead >> 3) == 0x1E ? 4 : 1;
			if (i + l_length > p_text.size()) {
				l_length = 1;
			}
			char32_t l_code = l_length == 1 ? l_lead : static_cast<char32_t>(l_lead & (0xFF >> (l_length + 1)));
			for (std::size_t k = 1; k < l_length; k++) {
				l_code = (l_code << 6) | (static_cast<unsigned char>(p_text[i + k]) & 0x3F);
			}
			if (is_white_space(l_code)) {
				l_words.emplace_back();
			} else {
				l_words.back().append(p_text, i, l_length);
			}
			i += l_length;
		}
		return l_words;
	}

} // namespace

namespace {

	const std::string s_no_text;
	const json s_no_json;
	const std::vector<std::uint8_t> s_no_bytes;

} // namespace

cam::Message::Message(AgentId p_sender, AgentId p_receiver, const json& p_message) :
		m_sender(p_sender),
		m_receiver(p_receiver),
		m_content(std::shared_ptr<const json>(std::allocate_shared<json>(RecyclingAllocator<json>(), p_message))) {
}

cam::Message::Message(AgentId p_sender, AgentId p_receiver, std::shared_ptr<const json> p_message) :
		m_sender(p_sender),
		m_receiver(p_receiver),
		m_content(p_message ? MessageContent(std::move(p_message)) : MessageContent()) {
}

cam::Message::Message(AgentId p_sender, AgentId p_receiver, std::string p_text) :
		m_sender(p_sender),
		m_receiver(p_receiver),
		m_content(std::move(p_text)) {
}

cam::Message::Message(AgentId p_sender, AgentId p_receiver, const uint8_t* p_message, const size_t& p_length) :
		m_sender(p_sender),
		m_receiver(p_receiver),
		m_content(p_message ? std::vector(p_message, p_message + p_length) : std::vector<std::uint8_t>()) {
}

cam::Message::Message(AgentId p_sender, AgentId p_receiver, MessageContent p_content) :
		m_sender(p_sender),
		m_receiver(p_receiver),
		m_content(std::move(p_content)) {
}

cam::Message::Message(Key, const AgentId& p_sender, const std::shared_ptr<const std::string>& p_sender_name, const AgentId& p_receiver,
					  MessageContent&& p_content, const std::string& p_sender_environment) :
		m_sender(p_sender),
		m_sender_name(p_sender_name),
		m_sender_environment(p_sender_environment),
		m_receiver(p_receiver),
		m_content(std::move(p_content)) {
}

cam::Text::Text(std::string p_text) :
		m_text(std::allocate_shared<std::string>(RecyclingAllocator<std::string>(), std::move(p_text))) {
}

cam::Text::Text(const char* p_text) :
		Text(std::string(p_text ? p_text : "")) {
}

void* cam::Message::operator new([[maybe_unused]] const std::size_t p_size) {
	// A final class: the size asked is always that of a message
	return FreeBlocks<sizeof(Message)>::acquire();
}

void cam::Message::operator delete(void* p_pointer) noexcept {
	FreeBlocks<sizeof(Message)>::release(p_pointer);
}

cam::Message::Message() :
		m_sender(),
		m_receiver(),
		m_content() {
}

const std::string& cam::Message::get_sender_name() const {
	return m_sender_name ? *m_sender_name : s_no_text;
}

cam::ContentKind cam::Message::kind() const {
	switch (m_content.index()) {
		case 1: return ContentKind::Text;
		case 2: return ContentKind::Json;
		case 3: return ContentKind::Bytes;
		case 4: return ContentKind::Text;
		default: return ContentKind::None;
	}
}

const std::string& cam::Message::as_text() const {
	if (const auto* l_text = std::get_if<std::string>(&m_content)) {
		return *l_text;
	}
	const auto* l_shared = std::get_if<Text>(&m_content);
	return l_shared ? l_shared->str() : s_no_text;
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

std::pair<std::string, std::vector<std::string>> cam::Message::parse() const {
	std::vector<std::string> l_words = split_words(to_string());
	std::string l_action = std::move(l_words.front());
	l_words.erase(l_words.begin());
	return {std::move(l_action), std::move(l_words)};
}

std::pair<std::string, std::string> cam::Message::parse_rest() const {
	const std::vector<std::string> l_words = split_words(to_string());
	std::string l_rest;
	for (std::size_t i = 1; i < l_words.size(); i++) {
		l_rest += (i == 1 ? "" : " ") + l_words[i];
	}
	return {l_words.front(), std::move(l_rest)};
}

std::pair<std::string, std::string> cam::Message::parse_one() const {
	const std::vector<std::string> l_words = split_words(to_string());
	if (l_words.size() < 2) {
		throw std::out_of_range("Message::parse_one: a single word in \"" + to_string() + "\"");
	}
	return {l_words[0], l_words[1]};
}

std::string cam::Message::format() const {
	return "[" + m_sender.str() + " -> " + m_receiver.str() + "]: " + to_string();
}
