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

#include "ActressMasCodec.h"

#include <array>
#include <bit>
#include <charconv>
#include <limits>
#include <map>
#include <optional>

#include "../../util/Base64.h"

namespace cam::transport {

	namespace {

		// Record types of MS-NRBF (2.1.2.1) that BinaryFormatter writes for a ContainerMessage and a Message
		constexpr std::uint8_t SERIALIZED_STREAM_HEADER = 0x00;
		constexpr std::uint8_t CLASS_WITH_MEMBERS_AND_TYPES = 0x05;
		constexpr std::uint8_t BINARY_OBJECT_STRING = 0x06;
		constexpr std::uint8_t MEMBER_PRIMITIVE_TYPED = 0x08;
		constexpr std::uint8_t MEMBER_REFERENCE = 0x09;
		constexpr std::uint8_t OBJECT_NULL = 0x0A;
		constexpr std::uint8_t MESSAGE_END = 0x0B;
		constexpr std::uint8_t BINARY_LIBRARY = 0x0C;

		// Binary types of the members (2.1.2.2): String, and Object for the dynamic ContentObj
		constexpr std::uint8_t STRING_TYPE = 1;
		constexpr std::uint8_t OBJECT_TYPE = 2;

		// Primitive types accepted in a boxed ContentObj (2.1.2.3)
		constexpr std::uint8_t BOOLEAN = 1;
		constexpr std::uint8_t DOUBLE = 6;
		constexpr std::uint8_t INT32 = 8;
		constexpr std::uint8_t INT64 = 9;

		// Identifiers as BinaryFormatter numbers them: the root object, its library, then each string in order
		constexpr std::int32_t ROOT_ID = 1;
		constexpr std::int32_t LIBRARY_ID = 2;
		constexpr std::int32_t FIRST_STRING_ID = 3;

		constexpr std::string_view CONTAINER_CLASS = "ActressMas.ContainerMessage";
		constexpr std::array<std::string_view, 4> CONTAINER_MEMBERS = {"<Content>k__BackingField", "<Info>k__BackingField",
																	   "<Receiver>k__BackingField", "<Sender>k__BackingField"};
		constexpr std::array<std::uint8_t, 4> CONTAINER_TYPES = {STRING_TYPE, STRING_TYPE, STRING_TYPE, STRING_TYPE};

		constexpr std::string_view MESSAGE_CLASS = "ActressMas.Message";
		constexpr std::array<std::string_view, 5> MESSAGE_MEMBERS = {"<Content>k__BackingField", "<ContentObj>k__BackingField",
																	 "<ConversationId>k__BackingField", "<Receiver>k__BackingField",
																	 "<Sender>k__BackingField"};
		constexpr std::array<std::uint8_t, 5> MESSAGE_TYPES = {STRING_TYPE, OBJECT_TYPE, STRING_TYPE, STRING_TYPE, STRING_TYPE};

		// Frame marker of AsyncClientServerLib (BasicHeader)
		constexpr std::uint8_t FRAME_BYTE_1 = 0x01;
		constexpr std::uint8_t FRAME_BYTE_2 = 0xFF;

		Error invalid(std::string p_message) {
			return make_error(ErrorCategory::Validation, ErrorCode::InvalidEnvelope, std::move(p_message));
		}

		/**
		 * MS-NRBF writer of one class record and its string members, as BinaryFormatter lays them out.
		 **/
		class Writer final {
		private:
			/**
			 * The bytes written so far.
			 **/
			std::vector<std::uint8_t> m_out;

			/**
			 * Identifier of the next string.
			 **/
			std::int32_t m_next_id = FIRST_STRING_ID;

			void write_byte(const std::uint8_t p_value) {
				m_out.push_back(p_value);
			}

			void write_int32(const std::int32_t p_value) {
				const auto l_value = static_cast<std::uint32_t>(p_value);
				for (int i = 0; i < 4; i++) {
					m_out.push_back(static_cast<std::uint8_t>(l_value >> (8 * i)));
				}
			}

			// Length in UTF-8 bytes, 7 bits per byte, low bits first (2.1.1.6)
			void write_length_prefixed(const std::string_view p_text) {
				auto l_length = static_cast<std::uint32_t>(p_text.size());
				while (l_length >= 0x80) {
					write_byte(static_cast<std::uint8_t>(l_length | 0x80));
					l_length >>= 7;
				}
				write_byte(static_cast<std::uint8_t>(l_length));
				m_out.insert(m_out.end(), p_text.begin(), p_text.end());
			}

		public:
			/**
			 * Write the stream header, the library and the class record.
			 * @param p_class Full name of the class
			 * @param p_members Names of its members, in the order of the values
			 * @param p_types Binary type of each member
			 **/
			Writer(const std::string_view p_class, const std::span<const std::string_view> p_members, const std::span<const std::uint8_t> p_types) {
				write_byte(SERIALIZED_STREAM_HEADER);
				write_int32(ROOT_ID);
				write_int32(-1);
				write_int32(1);
				write_int32(0);
				write_byte(BINARY_LIBRARY);
				write_int32(LIBRARY_ID);
				write_length_prefixed(ActressMasCodec::ASSEMBLY);
				write_byte(CLASS_WITH_MEMBERS_AND_TYPES);
				write_int32(ROOT_ID);
				write_length_prefixed(p_class);
				write_int32(static_cast<std::int32_t>(p_members.size()));
				for (const auto& l_member: p_members) {
					write_length_prefixed(l_member);
				}
				for (const auto l_type: p_types) {
					write_byte(l_type);
				}
				write_int32(LIBRARY_ID);
			}

			/**
			 * Write the value of a member: a string.
			 * @param p_value The string
			 **/
			void write_string(const std::string_view p_value) {
				write_byte(BINARY_OBJECT_STRING);
				write_int32(m_next_id++);
				write_length_prefixed(p_value);
			}

			/**
			 * Write the value of a member: null.
			 **/
			void write_null() {
				write_byte(OBJECT_NULL);
			}

			/**
			 * End the stream.
			 * @return The bytes
			 **/
			std::vector<std::uint8_t> finish() {
				write_byte(MESSAGE_END);
				return std::move(m_out);
			}
		};

		/**
		 * MS-NRBF reader of one class record and its members, bounds-checked; the first failure is sticky.
		 **/
		class Reader final {
		private:
			/**
			 * The bytes to read.
			 **/
			std::span<const std::uint8_t> m_in;

			/**
			 * Position of the next byte.
			 **/
			std::size_t m_position = 0;

			/**
			 * First failure, if any.
			 **/
			std::optional<Error> m_error;

			/**
			 * Strings read so far, by object id, for the member references.
			 **/
			std::map<std::int32_t, std::string> m_strings;

			bool need(const std::size_t p_count) {
				if (m_error) {
					return false;
				}
				if (p_count > m_in.size() - m_position) {
					m_error = invalid("truncated BinaryFormatter stream");
					return false;
				}
				return true;
			}

			std::uint64_t read_little_endian(const int p_size) {
				if (!need(static_cast<std::size_t>(p_size))) {
					return 0;
				}
				std::uint64_t l_value = 0;
				for (int i = 0; i < p_size; i++) {
					l_value |= static_cast<std::uint64_t>(m_in[m_position++]) << (8 * i);
				}
				return l_value;
			}

			std::int32_t read_int32() {
				return static_cast<std::int32_t>(static_cast<std::uint32_t>(read_little_endian(4)));
			}

			std::string read_length_prefixed() {
				std::uint32_t l_length = 0;
				for (int i = 0; i < 5; i++) {
					const std::uint8_t l_byte = read_byte();
					if (m_error) {
						return {};
					}
					// The fifth byte holds the last 3 bits of a 31-bit length
					if (i == 4 && l_byte > 0x07) {
						fail("length prefix out of range");
						return {};
					}
					l_length |= static_cast<std::uint32_t>(l_byte & 0x7F) << (7 * i);
					if ((l_byte & 0x80) == 0) {
						if (!need(l_length)) {
							return {};
						}
						const auto l_begin = m_in.begin() + static_cast<std::ptrdiff_t>(m_position);
						m_position += l_length;
						return std::string(l_begin, l_begin + static_cast<std::ptrdiff_t>(l_length));
					}
				}
				fail("length prefix longer than 5 bytes");
				return {};
			}

			std::string read_object_string() {
				const std::int32_t l_id = read_int32();
				std::string l_value = read_length_prefixed();
				if (!m_error && !m_strings.emplace(l_id, l_value).second) {
					fail("object id used twice");
				}
				return l_value;
			}

			std::string read_reference() {
				const std::int32_t l_id = read_int32();
				if (m_error) {
					return {};
				}
				const auto l_string = m_strings.find(l_id);
				if (l_string == m_strings.end()) {
					fail("reference to an unknown string");
					return {};
				}
				return l_string->second;
			}

			// A boxed value as C# would print it (bool.ToString, shortest round-trip double)
			std::string read_primitive() {
				switch (read_byte()) {
					case BOOLEAN:
						return read_byte() != 0 ? "True" : "False";
					case INT32:
						return std::to_string(read_int32());
					case INT64:
						return std::to_string(static_cast<std::int64_t>(read_little_endian(8)));
					case DOUBLE: {
						const auto l_value = std::bit_cast<double>(read_little_endian(8));
						std::array<char, 32> l_text{};
						const auto l_result = std::to_chars(l_text.data(), l_text.data() + l_text.size(), l_value);
						return std::string(l_text.data(), l_result.ptr);
					}
					default:
						fail("unsupported primitive type in ContentObj");
						return {};
				}
			}

		public:
			/**
			 * Start reading.
			 * @param p_in The bytes
			 **/
			explicit Reader(const std::span<const std::uint8_t> p_in) : m_in(p_in) {}

			/**
			 * The first failure, if any.
			 * @return The error
			 **/
			[[nodiscard]] const std::optional<Error>& get_error() const { return m_error; }

			/**
			 * Record a failure, unless one is already recorded.
			 * @param p_message What went wrong
			 **/
			void fail(std::string p_message) {
				if (!m_error) {
					m_error = invalid(std::move(p_message));
				}
			}

			/**
			 * Read one byte.
			 * @return The byte, or 0 after a failure
			 **/
			std::uint8_t read_byte() {
				if (!need(1)) {
					return 0;
				}
				return m_in[m_position++];
			}

			/**
			 * Read the stream header, the library and the class record, and check that they are the expected ones.
			 * @param p_class Full name of the class
			 * @param p_members Names of its members
			 * @param p_types Binary type of each member
			 **/
			void read_class(const std::string_view p_class, const std::span<const std::string_view> p_members,
							const std::span<const std::uint8_t> p_types) {
				if (read_byte() != SERIALIZED_STREAM_HEADER) {
					fail("not a BinaryFormatter stream");
				}
				const std::int32_t l_root = read_int32();
				read_int32();  // header id, never used by BinaryFormatter
				const std::int32_t l_major = read_int32();
				const std::int32_t l_minor = read_int32();
				if (l_major != 1 || l_minor != 0) {
					fail("unsupported BinaryFormatter stream version");
				}
				if (read_byte() != BINARY_LIBRARY) {
					fail("library record expected");
				}
				const std::int32_t l_library = read_int32();
				if (read_length_prefixed() != ActressMasCodec::ASSEMBLY) {
					fail("unknown assembly: ActressMas 3.1.0.0 expected");
				}
				if (read_byte() != CLASS_WITH_MEMBERS_AND_TYPES) {
					fail("class record expected");
				}
				if (read_int32() != l_root) {
					fail("the root object is not the class record");
				}
				if (read_length_prefixed() != p_class) {
					fail("unexpected class");
				}
				if (read_int32() != static_cast<std::int32_t>(p_members.size())) {
					fail("unexpected member count");
				}
				for (const auto& l_member: p_members) {
					if (read_length_prefixed() != l_member) {
						fail("unexpected member");
					}
				}
				for (const auto l_type: p_types) {
					if (read_byte() != l_type) {
						fail("unexpected member type");
					}
				}
				if (read_int32() != l_library) {
					fail("unexpected library id");
				}
			}

			/**
			 * Read the value of a String member.
			 * @return The string, or nothing for null
			 **/
			std::optional<std::string> read_string_member() {
				switch (read_byte()) {
					case BINARY_OBJECT_STRING:
						return read_object_string();
					case MEMBER_REFERENCE:
						return read_reference();
					case OBJECT_NULL:
						return std::nullopt;
					default:
						fail("unexpected record in a string member");
						return std::nullopt;
				}
			}

			/**
			 * Read the value of the Object member ContentObj: null, a string or a boxed primitive.
			 * @return The value as text, or nothing for null
			 **/
			std::optional<std::string> read_object_member() {
				switch (read_byte()) {
					case OBJECT_NULL:
						return std::nullopt;
					case BINARY_OBJECT_STRING:
						return read_object_string();
					case MEMBER_REFERENCE:
						return read_reference();
					case MEMBER_PRIMITIVE_TYPED:
						return read_primitive();
					default:
						fail("unsupported ContentObj: only a string, a number or a boolean can be read");
						return std::nullopt;
				}
			}

			/**
			 * Read the end of the stream; nothing may follow.
			 **/
			void read_end() {
				if (read_byte() != MESSAGE_END) {
					fail("end of the BinaryFormatter stream expected");
				}
				if (!m_error && m_position != m_in.size()) {
					fail("bytes after the end of the BinaryFormatter stream");
				}
			}
		};

	} // namespace

	std::vector<std::uint8_t> ActressMasCodec::encode_frame(const ContainerMessage& p_message) {
		const std::string l_base64 = to_base64(write_nrbf(p_message));
		const std::size_t l_size = HEADER_SIZE + 2 * l_base64.size();
		std::vector<std::uint8_t> l_frame;
		l_frame.reserve(l_size);
		l_frame.push_back(FRAME_BYTE_1);
		l_frame.push_back(FRAME_BYTE_2);
		for (int i = 3; i >= 0; i--) {
			l_frame.push_back(static_cast<std::uint8_t>(static_cast<std::uint32_t>(l_size) >> (8 * i)));
		}
		// The message and client GUIDs are only traced by the C# side: zeros
		l_frame.resize(HEADER_SIZE, 0);
		// UTF-16LE of an ASCII text: each character, then a zero byte
		for (const char l_char: l_base64) {
			l_frame.push_back(static_cast<std::uint8_t>(l_char));
			l_frame.push_back(0);
		}
		return l_frame;
	}

	Result<std::size_t> ActressMasCodec::read_frame_size(const std::span<const std::uint8_t> p_header, const std::size_t p_max_bytes) {
		if (p_header.size() < HEADER_SIZE) {
			return invalid("truncated frame header");
		}
		if (p_header[0] != FRAME_BYTE_1 || p_header[1] != FRAME_BYTE_2) {
			return invalid("not an ActressMAS frame");
		}
		std::size_t l_size = 0;
		for (std::size_t i = 2; i < 6; i++) {
			l_size = (l_size << 8) | p_header[i];
		}
		if (l_size < HEADER_SIZE) {
			return invalid("frame shorter than its header");
		}
		if (l_size > p_max_bytes) {
			return make_error(ErrorCategory::Validation, ErrorCode::TooLarge, "frame too large");
		}
		return l_size;
	}

	Result<ContainerMessage> ActressMasCodec::decode_frame(const std::span<const std::uint8_t> p_frame, const std::size_t p_max_bytes) {
		const auto l_size = read_frame_size(p_frame, p_max_bytes);
		if (!l_size) {
			return l_size.error();
		}
		if (l_size.value() != p_frame.size()) {
			return invalid("frame size does not match its header");
		}
		const auto l_body = p_frame.subspan(HEADER_SIZE);
		if (l_body.size() % 2 != 0) {
			return invalid("frame body is not UTF-16");
		}
		std::string l_base64;
		l_base64.reserve(l_body.size() / 2);
		for (std::size_t i = 0; i < l_body.size(); i += 2) {
			if (l_body[i + 1] != 0 || l_body[i] >= 0x80) {
				return invalid("frame body is not ASCII text");
			}
			l_base64.push_back(static_cast<char>(l_body[i]));
		}
		const auto l_bytes = from_base64(l_base64);
		if (!l_bytes) {
			return invalid("frame body is not valid base64");
		}
		return read_container_nrbf(l_bytes.value());
	}

	std::string ActressMasCodec::encode_remote_message(const RemoteMessage& p_message) {
		return to_base64(write_nrbf(p_message));
	}

	Result<RemoteMessage> ActressMasCodec::decode_remote_message(const std::string_view p_base64) {
		const auto l_bytes = from_base64(p_base64);
		if (!l_bytes) {
			return invalid("message content is not valid base64");
		}
		return read_remote_nrbf(l_bytes.value());
	}

	std::vector<std::uint8_t> ActressMasCodec::write_nrbf(const ContainerMessage& p_message) {
		Writer l_writer(CONTAINER_CLASS, CONTAINER_MEMBERS, CONTAINER_TYPES);
		l_writer.write_string(p_message.m_content);
		l_writer.write_string(p_message.m_info);
		l_writer.write_string(p_message.m_receiver);
		l_writer.write_string(p_message.m_sender);
		return l_writer.finish();
	}

	std::vector<std::uint8_t> ActressMasCodec::write_nrbf(const RemoteMessage& p_message) {
		Writer l_writer(MESSAGE_CLASS, MESSAGE_MEMBERS, MESSAGE_TYPES);
		l_writer.write_string(p_message.m_content);
		l_writer.write_null();  // ContentObj: Agent.Send refuses objects for remote agents
		l_writer.write_string(p_message.m_conversation_id);
		l_writer.write_string(p_message.m_receiver);
		l_writer.write_string(p_message.m_sender);
		return l_writer.finish();
	}

	Result<ContainerMessage> ActressMasCodec::read_container_nrbf(const std::span<const std::uint8_t> p_bytes) {
		Reader l_reader(p_bytes);
		l_reader.read_class(CONTAINER_CLASS, CONTAINER_MEMBERS, CONTAINER_TYPES);
		ContainerMessage l_message;
		l_message.m_content = l_reader.read_string_member().value_or("");
		l_message.m_info = l_reader.read_string_member().value_or("");
		l_message.m_receiver = l_reader.read_string_member().value_or("");
		l_message.m_sender = l_reader.read_string_member().value_or("");
		l_reader.read_end();
		if (l_reader.get_error()) {
			return l_reader.get_error().value();
		}
		return l_message;
	}

	Result<RemoteMessage> ActressMasCodec::read_remote_nrbf(const std::span<const std::uint8_t> p_bytes) {
		Reader l_reader(p_bytes);
		l_reader.read_class(MESSAGE_CLASS, MESSAGE_MEMBERS, MESSAGE_TYPES);
		RemoteMessage l_message;
		const auto l_content = l_reader.read_string_member();
		const auto l_object = l_reader.read_object_member();
		// Format() prints Content; a message built with an object only has its ContentObj
		l_message.m_content = l_content ? l_content.value() : l_object.value_or("");
		l_message.m_conversation_id = l_reader.read_string_member().value_or("");
		l_message.m_receiver = l_reader.read_string_member().value_or("");
		l_message.m_sender = l_reader.read_string_member().value_or("");
		l_reader.read_end();
		if (l_reader.get_error()) {
			return l_reader.get_error().value();
		}
		return l_message;
	}

} // namespace cam::transport
