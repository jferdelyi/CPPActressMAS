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

cam::Message::Message(std::string p_sender, std::string p_receiver, const json& p_message,
					  const MessageBinaryFormat& p_binary_format) :
		m_sender(std::move(p_sender)),
		m_receiver(std::move(p_receiver)),
		m_binary_format(p_binary_format),
		m_binary_message(cam::Message::to_binary(p_message, m_binary_format)) {
}

cam::Message::Message(std::string p_sender, std::string p_receiver, const uint8_t* p_message, const size_t& p_length,
					  const MessageBinaryFormat& p_binary_format) :
		m_sender(std::move(p_sender)),
		m_receiver(std::move(p_receiver)),
		m_binary_format(p_binary_format) {
	if (p_message) {
		m_binary_message = std::vector(p_message, p_message + p_length);
	}
}

cam::Message::Message() :
		m_sender(),
		m_receiver(),
		m_binary_format(MessageBinaryFormat::MessagePack),
		m_binary_message() {
}

json cam::Message::content() const {
	return cam::Message::to_json(m_binary_message, m_binary_format);
}

std::string cam::Message::to_string() const {
	if (m_binary_format == MessageBinaryFormat::RAW) {
		return content();
	}
	return content().dump();
}

std::string cam::Message::format() const {
	return "[" + m_sender + " -> " + m_receiver + "]: " + to_string();
}

json
cam::Message::to_json(const std::vector<std::uint8_t>& p_binary_message, const MessageBinaryFormat& p_binary_format) {
	switch (p_binary_format) {
		case MessageBinaryFormat::BJData:
			return json::from_bjdata(p_binary_message);

		case MessageBinaryFormat::BSON:
			return json::from_bson(p_binary_message);

		case MessageBinaryFormat::CBOR:
			return json::from_cbor(p_binary_message);

		case MessageBinaryFormat::UBJSON:
			return json::from_ubjson(p_binary_message);

		case MessageBinaryFormat::RAW:
			return std::string(p_binary_message.begin(), p_binary_message.end());

		case MessageBinaryFormat::MessagePack:
		default:
			return json::from_msgpack(p_binary_message);
	}
}

std::vector<std::uint8_t> cam::Message::to_binary(const json& p_message, const MessageBinaryFormat& p_binary_format) {
	switch (p_binary_format) {
		case MessageBinaryFormat::BJData:
			return json::to_bjdata(p_message);

		case MessageBinaryFormat::BSON:
			return json::to_bson(p_message);

		case MessageBinaryFormat::CBOR:
			return json::to_cbor(p_message);

		case MessageBinaryFormat::UBJSON:
			return json::to_ubjson(p_message);

		case MessageBinaryFormat::MessagePack:
		default:
			return json::to_msgpack(p_message);
	}
}