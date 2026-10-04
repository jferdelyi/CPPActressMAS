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

#include "ContentCodec.h"

namespace cam::transport {

	namespace {

		Error invalid(std::string p_message) {
			return make_error(ErrorCategory::Validation, ErrorCode::InvalidEnvelope, std::move(p_message));
		}

		bool is_json_encoding(const MessageBinaryFormat p_encoding) {
			switch (p_encoding) {
				case MessageBinaryFormat::BJData:
				case MessageBinaryFormat::BSON:
				case MessageBinaryFormat::CBOR:
				case MessageBinaryFormat::MessagePack:
				case MessageBinaryFormat::UBJSON:
				case MessageBinaryFormat::JSON:
					return true;
				case MessageBinaryFormat::RAW:
				case MessageBinaryFormat::Text:
				default:
					return false;
			}
		}

		std::vector<std::uint8_t> encode_json(const json& p_value, const MessageBinaryFormat p_encoding) {
			switch (p_encoding) {
				case MessageBinaryFormat::BJData: return json::to_bjdata(p_value);
				case MessageBinaryFormat::BSON: return json::to_bson(p_value);
				case MessageBinaryFormat::CBOR: return json::to_cbor(p_value);
				case MessageBinaryFormat::UBJSON: return json::to_ubjson(p_value);
				case MessageBinaryFormat::JSON: {
					const std::string l_text = p_value.dump();
					return {l_text.begin(), l_text.end()};
				}
				case MessageBinaryFormat::MessagePack:
				default: return json::to_msgpack(p_value);
			}
		}

		json decode_json(const std::span<const std::uint8_t> p_bytes, const MessageBinaryFormat p_encoding) {
			switch (p_encoding) {
				case MessageBinaryFormat::BJData: return json::from_bjdata(p_bytes.begin(), p_bytes.end());
				case MessageBinaryFormat::BSON: return json::from_bson(p_bytes.begin(), p_bytes.end());
				case MessageBinaryFormat::CBOR: return json::from_cbor(p_bytes.begin(), p_bytes.end());
				case MessageBinaryFormat::UBJSON: return json::from_ubjson(p_bytes.begin(), p_bytes.end());
				case MessageBinaryFormat::JSON: return json::parse(p_bytes.begin(), p_bytes.end());
				case MessageBinaryFormat::MessagePack:
				default: return json::from_msgpack(p_bytes.begin(), p_bytes.end());
			}
		}

	} // namespace

	Result<MessageBinaryFormat> wire_encoding(const MessageContent& p_content, const SendOptions& p_options) {
		switch (p_content.index()) {
			case 1:  // text
				return MessageBinaryFormat::Text;
			case 2: {  // JSON value: the binary encoding of the options, MessagePack by default
				const auto l_encoding = p_options.m_encoding.value_or(MessageBinaryFormat::MessagePack);
				if (!is_json_encoding(l_encoding)) {
					return invalid("a JSON content cannot travel with the RAW or Text encoding");
				}
				return l_encoding;
			}
			case 3:  // bytes: opaque, unless the sender tags them
				return p_options.m_encoding.value_or(MessageBinaryFormat::RAW);
			default:
				return MessageBinaryFormat::RAW;
		}
	}

	Result<std::vector<std::uint8_t>> encode_content(const MessageContent& p_content, const MessageBinaryFormat p_encoding) {
		if (const auto* l_text = std::get_if<std::string>(&p_content)) {
			return std::vector<std::uint8_t>(l_text->begin(), l_text->end());
		}
		if (const auto* l_json = std::get_if<std::shared_ptr<const json>>(&p_content)) {
			if (!is_json_encoding(p_encoding)) {
				return invalid("a JSON content cannot travel with the RAW or Text encoding");
			}
			return encode_json(*l_json ? **l_json : json(), p_encoding);
		}
		if (const auto* l_bytes = std::get_if<std::vector<std::uint8_t>>(&p_content)) {
			return *l_bytes;
		}
		return std::vector<std::uint8_t>();
	}

	Result<MessageContent> decode_content(const std::span<const std::uint8_t> p_bytes, const MessageBinaryFormat p_encoding) {
		switch (p_encoding) {
			case MessageBinaryFormat::Text:
				return MessageContent(std::string(p_bytes.begin(), p_bytes.end()));
			case MessageBinaryFormat::RAW:
				return MessageContent(std::vector<std::uint8_t>(p_bytes.begin(), p_bytes.end()));
			default:
				break;
		}
		if (!is_json_encoding(p_encoding)) {
			return invalid("unknown content encoding");
		}
		if (p_bytes.empty()) {
			return MessageContent(std::make_shared<const json>());
		}
		try {
			return MessageContent(std::make_shared<const json>(decode_json(p_bytes, p_encoding)));
		} catch (const json::exception& p_exception) {
			return invalid(std::string("content does not match its encoding: ") + p_exception.what());
		}
	}

} // namespace cam::transport
