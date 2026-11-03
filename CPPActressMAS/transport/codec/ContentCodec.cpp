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

#include "ContentCodec.h"

namespace cam::transport {

	namespace {

		Error invalid(std::string p_message) {
			return make_error(ErrorCategory::Validation, ErrorCode::InvalidEnvelope, std::move(p_message));
		}

		bool is_json_encoding(const ContentEncoding p_encoding) {
			switch (p_encoding) {
				case ContentEncoding::BjData:
				case ContentEncoding::Bson:
				case ContentEncoding::Cbor:
				case ContentEncoding::MessagePack:
				case ContentEncoding::Ubjson:
				case ContentEncoding::Json:
					return true;
				case ContentEncoding::Raw:
				case ContentEncoding::Text:
				default:
					return false;
			}
		}

		std::vector<std::uint8_t> encode_json(const json& p_value, const ContentEncoding p_encoding) {
			switch (p_encoding) {
				case ContentEncoding::BjData: return json::to_bjdata(p_value);
				case ContentEncoding::Bson: return json::to_bson(p_value);
				case ContentEncoding::Cbor: return json::to_cbor(p_value);
				case ContentEncoding::Ubjson: return json::to_ubjson(p_value);
				case ContentEncoding::Json: {
					const std::string l_text = p_value.dump();
					return {l_text.begin(), l_text.end()};
				}
				case ContentEncoding::MessagePack:
				default: return json::to_msgpack(p_value);
			}
		}

		json decode_json(const std::span<const std::uint8_t> p_bytes, const ContentEncoding p_encoding) {
			switch (p_encoding) {
				case ContentEncoding::BjData: return json::from_bjdata(p_bytes.begin(), p_bytes.end());
				case ContentEncoding::Bson: return json::from_bson(p_bytes.begin(), p_bytes.end());
				case ContentEncoding::Cbor: return json::from_cbor(p_bytes.begin(), p_bytes.end());
				case ContentEncoding::Ubjson: return json::from_ubjson(p_bytes.begin(), p_bytes.end());
				case ContentEncoding::Json: return json::parse(p_bytes.begin(), p_bytes.end());
				case ContentEncoding::MessagePack:
				default: return json::from_msgpack(p_bytes.begin(), p_bytes.end());
			}
		}

	} // namespace

	Result<ContentEncoding> wire_encoding(const MessageContent& p_content, const SendOptions& p_options) {
		switch (p_content.index()) {
			case 1:  // text
				return ContentEncoding::Text;
			case 2: {  // JSON value: the binary encoding of the options, MessagePack by default
				const auto l_encoding = p_options.m_encoding.value_or(ContentEncoding::MessagePack);
				if (!is_json_encoding(l_encoding)) {
					return invalid("a JSON content cannot travel with the Raw or Text encoding");
				}
				return l_encoding;
			}
			case 3:  // bytes: opaque, unless the sender tags them
				return p_options.m_encoding.value_or(ContentEncoding::Raw);
			case 4:  // shared text
				return ContentEncoding::Text;
			default:
				return ContentEncoding::Raw;
		}
	}

	Result<std::vector<std::uint8_t>> encode_content(const MessageContent& p_content, const ContentEncoding p_encoding) {
		if (const auto* l_text = std::get_if<std::string>(&p_content)) {
			return std::vector<std::uint8_t>(l_text->begin(), l_text->end());
		}
		if (const auto* l_json = std::get_if<std::shared_ptr<const json>>(&p_content)) {
			if (!is_json_encoding(p_encoding)) {
				return invalid("a JSON content cannot travel with the Raw or Text encoding");
			}
			return encode_json(*l_json ? **l_json : json(), p_encoding);
		}
		if (const auto* l_bytes = std::get_if<std::vector<std::uint8_t>>(&p_content)) {
			return *l_bytes;
		}
		if (const auto* l_shared = std::get_if<Text>(&p_content)) {
			return std::vector<std::uint8_t>(l_shared->str().begin(), l_shared->str().end());
		}
		return std::vector<std::uint8_t>();
	}

	Result<MessageContent> decode_content(const std::span<const std::uint8_t> p_bytes, const ContentEncoding p_encoding) {
		switch (p_encoding) {
			case ContentEncoding::Text:
				return MessageContent(std::string(p_bytes.begin(), p_bytes.end()));
			case ContentEncoding::Raw:
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
