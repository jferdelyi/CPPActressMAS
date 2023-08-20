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

cam::Message::Message(const std::string& p_sender, const std::string& p_receiver, const json& p_message, const cam::MessageBinaryFormat& p_binary_format) : 
	m_sender(p_sender),
	m_receiver(p_receiver),
	m_binary_format(p_binary_format) {
	switch (m_binary_format) {
		case MessageBinaryFormat::BJData:
			m_binary_message = json::to_bjdata(p_message);
			break;

		case MessageBinaryFormat::BSON:
			m_binary_message = json::to_bson(p_message);
			break;

		case MessageBinaryFormat::CBOR:
			m_binary_message = json::to_cbor(p_message);
			break;
			
		case MessageBinaryFormat::UBJSON:
			m_binary_message = json::to_ubjson(p_message);
			break;
		
		case MessageBinaryFormat::MessagePack:
		default:
			m_binary_message = json::to_msgpack(p_message);
			break;
	}
}

const std::string& cam::Message::get_sender() const {
	return m_sender;
}

const std::string& cam::Message::get_receiver() const {
	return m_receiver;
}

const std::vector<std::uint8_t>& cam::Message::get_binary_message() const {
	return m_binary_message;
}

const json cam::Message::content() const {
	switch (m_binary_format) {
		case cam::MessageBinaryFormat::BJData:
			return json::from_bjdata(m_binary_message);

		case cam::MessageBinaryFormat::BSON:
			return json::from_bson(m_binary_message);

		case cam::MessageBinaryFormat::CBOR:
			return json::from_cbor(m_binary_message);
			
		case cam::MessageBinaryFormat::UBJSON:
			return json::from_ubjson(m_binary_message);
		
		case cam::MessageBinaryFormat::MessagePack:
		default:
			return json::from_msgpack(m_binary_message);
	}
}

const std::string cam::Message::to_string() const {
	return content().dump();
}

const std::string cam::Message::format() const {
	return "[" + m_sender + " -> " + m_receiver + "]: " + to_string();
}