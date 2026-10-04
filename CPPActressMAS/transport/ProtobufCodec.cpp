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

#include "ProtobufCodec.h"

namespace cam::transport {

	namespace {

		// Wire types of proto3.
		constexpr std::uint64_t WIRE_VARINT = 0;
		constexpr std::uint64_t WIRE_FIXED64 = 1;
		constexpr std::uint64_t WIRE_LEN = 2;
		constexpr std::uint64_t WIRE_FIXED32 = 5;

		// Field numbers of envelope.proto.
		namespace field {
			constexpr std::uint64_t VERSION_MAJOR = 1, VERSION_MINOR = 2, KIND = 3, SENDER_ENVIRONMENT = 4, ORIGIN_ENVIRONMENT = 5,
								   EPOCH = 6, STREAM_KIND = 7, STREAM_TARGET = 8, SEQUENCE = 9, CONVERSATION_ID = 10, SENDER_AGENT = 11,
								   TARGET_KIND = 12, TARGET_ID = 13, TARGET_NAME = 14, FIRST_ONLY = 15, SCOPE = 16, CONTENT_ENCODING = 17,
								   CONTENT = 18, PRIORITY = 19, TRANSFER = 20, CONTROL = 21, PRESENCE = 22, DIRECTORY = 23;
			constexpr std::uint64_t TRANSFER_ID = 1, TYPE_NAME = 2, AGENT_ID = 3, AGENT_NAME = 4, STATE_ENCODING = 5, STATE = 6, QUEUE = 7,
								   USES_OBSERVABLES = 8, OBSERVABLES = 9;
			constexpr std::uint64_t CONTROL_TRANSFER_ID = 1, CONTROL_AGENT_ID = 2, ACCEPTED = 3, REASON = 4;
			constexpr std::uint64_t NODE_ID = 1, ENVIRONMENT_ID = 2, ENVIRONMENT_NAME = 3, NAMESPACE = 4, ONLINE = 5, HAS_TURN = 6, TURN = 7,
								   TIMESTAMP_MS = 8, AGENTS = 9;
			constexpr std::uint64_t ENTRY_ID = 1, ENTRY_NAME = 2;
			constexpr std::uint64_t EVENT = 1, EVENT_AGENT_ID = 2, EVENT_AGENT_NAME = 3, EVENT_ENVIRONMENT_ID = 4, PREVIOUS_ENVIRONMENT_ID = 5;
		} // namespace field

		Error invalid(std::string p_message) {
			return make_error(ErrorCategory::Validation, ErrorCode::InvalidEnvelope, std::move(p_message));
		}

		// An enum value out of the byte range is kept out of range (255) so that validation refuses it.
		std::uint8_t enum_byte(const std::uint64_t p_value) {
			return p_value > 255 ? static_cast<std::uint8_t>(255) : static_cast<std::uint8_t>(p_value);
		}

		/**
		 * proto3 writer: defaults are omitted, as every proto3 encoder does.
		 **/
		class Writer final {
		private:
			std::vector<std::uint8_t>& m_out;

			void tag(const std::uint64_t p_field, const std::uint64_t p_wire_type) { varint((p_field << 3) | p_wire_type); }

		public:
			explicit Writer(std::vector<std::uint8_t>& p_out) : m_out(p_out) {}

			void varint(std::uint64_t p_value) {
				while (p_value >= 0x80) {
					m_out.push_back(static_cast<std::uint8_t>(p_value | 0x80));
					p_value >>= 7;
				}
				m_out.push_back(static_cast<std::uint8_t>(p_value));
			}

			void uint(const std::uint64_t p_field, const std::uint64_t p_value) {
				if (p_value != 0) {
					tag(p_field, WIRE_VARINT);
					varint(p_value);
				}
			}

			void boolean(const std::uint64_t p_field, const bool p_value) { uint(p_field, p_value ? 1 : 0); }

			void text(const std::uint64_t p_field, const std::string& p_text) {
				if (!p_text.empty()) {
					tag(p_field, WIRE_LEN);
					varint(p_text.size());
					m_out.insert(m_out.end(), p_text.begin(), p_text.end());
				}
			}

			void bytes(const std::uint64_t p_field, const std::vector<std::uint8_t>& p_bytes) {
				if (!p_bytes.empty()) {
					tag(p_field, WIRE_LEN);
					varint(p_bytes.size());
					m_out.insert(m_out.end(), p_bytes.begin(), p_bytes.end());
				}
			}

			// A nested message is always written, even empty: its presence carries meaning (oneof, repeated).
			void message(const std::uint64_t p_field, const std::vector<std::uint8_t>& p_bytes) {
				tag(p_field, WIRE_LEN);
				varint(p_bytes.size());
				m_out.insert(m_out.end(), p_bytes.begin(), p_bytes.end());
			}
		};

		/**
		 * proto3 reader over one message; bounds-checked, the first failure is sticky.
		 **/
		class Reader final {
		private:
			std::span<const std::uint8_t> m_in;
			std::size_t m_position = 0;
			std::optional<Error> m_error;

		public:
			explicit Reader(const std::span<const std::uint8_t> p_in) : m_in(p_in) {}

			[[nodiscard]] const std::optional<Error>& error() const { return m_error; }
			[[nodiscard]] bool finished() const { return m_error.has_value() || m_position >= m_in.size(); }

			void fail(std::string p_message) {
				if (!m_error) {
					m_error = invalid(std::move(p_message));
				}
			}

			void fail_size(std::string p_message) {
				if (!m_error) {
					m_error = make_error(ErrorCategory::Validation, ErrorCode::TooLarge, std::move(p_message));
				}
			}

			std::uint64_t varint() {
				std::uint64_t l_value = 0;
				for (int l_shift = 0; l_shift < 64; l_shift += 7) {
					if (m_position >= m_in.size()) {
						fail("truncated varint");
						return 0;
					}
					const std::uint8_t l_byte = m_in[m_position++];
					l_value |= static_cast<std::uint64_t>(l_byte & 0x7f) << l_shift;
					if ((l_byte & 0x80) == 0) {
						return l_value;
					}
				}
				fail("varint too long");
				return 0;
			}

			// Next tag: field number and wire type; false at the end of the message.
			bool next(std::uint64_t& p_field, std::uint64_t& p_wire_type) {
				if (finished()) {
					return false;
				}
				const std::uint64_t l_tag = varint();
				if (m_error) {
					return false;
				}
				p_field = l_tag >> 3;
				p_wire_type = l_tag & 7;
				if (p_field == 0) {
					fail("field number 0");
					return false;
				}
				return true;
			}

			// The bytes of a length-delimited field.
			std::span<const std::uint8_t> length_delimited() {
				const std::uint64_t l_size = varint();
				if (m_error) return {};
				if (l_size > m_in.size() - m_position) {
					fail("truncated field");
					return {};
				}
				const auto l_result = m_in.subspan(m_position, static_cast<std::size_t>(l_size));
				m_position += static_cast<std::size_t>(l_size);
				return l_result;
			}

			std::string text(const std::uint64_t p_wire_type) {
				if (p_wire_type != WIRE_LEN) {
					fail("string field with a wrong wire type");
					return {};
				}
				const auto l_bytes = length_delimited();
				if (m_error) return {};
				if (l_bytes.size() > ENVELOPE_MAX_STRING_BYTES) {
					fail_size("string too large");
					return {};
				}
				return std::string(l_bytes.begin(), l_bytes.end());
			}

			std::vector<std::uint8_t> bytes(const std::uint64_t p_wire_type, const std::size_t p_max) {
				if (p_wire_type != WIRE_LEN) {
					fail("bytes field with a wrong wire type");
					return {};
				}
				const auto l_bytes = length_delimited();
				if (m_error) return {};
				if (l_bytes.size() > p_max) {
					fail_size("byte array too large");
					return {};
				}
				return std::vector<std::uint8_t>(l_bytes.begin(), l_bytes.end());
			}

			std::span<const std::uint8_t> message(const std::uint64_t p_wire_type) {
				if (p_wire_type != WIRE_LEN) {
					fail("message field with a wrong wire type");
					return {};
				}
				return length_delimited();
			}

			std::uint64_t uint(const std::uint64_t p_wire_type) {
				if (p_wire_type != WIRE_VARINT) {
					fail("integer field with a wrong wire type");
					return 0;
				}
				return varint();
			}

			// Unknown field: skipped according to its wire type (groups are refused).
			void skip(const std::uint64_t p_wire_type) {
				switch (p_wire_type) {
					case WIRE_VARINT: (void) varint(); break;
					case WIRE_FIXED64: advance(8); break;
					case WIRE_LEN: (void) length_delimited(); break;
					case WIRE_FIXED32: advance(4); break;
					default: fail("unsupported wire type"); break;
				}
			}

		private:
			void advance(const std::size_t p_count) {
				if (p_count > m_in.size() - m_position) {
					fail("truncated field");
					return;
				}
				m_position += p_count;
			}
		};

		// ---------------------------------------------------------------- writing

		void write_envelope(Writer& p_writer, const Envelope& p_envelope);

		std::vector<std::uint8_t> encode_transfer(const AgentTransfer& p_transfer) {
			std::vector<std::uint8_t> l_out;
			Writer l_writer(l_out);
			l_writer.text(field::TRANSFER_ID, p_transfer.m_transfer_id);
			l_writer.text(field::TYPE_NAME, p_transfer.m_type_name);
			l_writer.text(field::AGENT_ID, p_transfer.m_agent_id);
			l_writer.text(field::AGENT_NAME, p_transfer.m_agent_name);
			l_writer.uint(field::STATE_ENCODING, static_cast<std::uint64_t>(p_transfer.m_state_encoding));
			l_writer.bytes(field::STATE, p_transfer.m_state);
			for (const auto& l_queued: p_transfer.m_queue) {
				std::vector<std::uint8_t> l_nested;
				Writer l_nested_writer(l_nested);
				write_envelope(l_nested_writer, l_queued);
				l_writer.message(field::QUEUE, l_nested);
			}
			l_writer.boolean(field::USES_OBSERVABLES, p_transfer.m_uses_observables);
			l_writer.bytes(field::OBSERVABLES, p_transfer.m_observables);
			return l_out;
		}

		std::vector<std::uint8_t> encode_control(const AgentControl& p_control) {
			std::vector<std::uint8_t> l_out;
			Writer l_writer(l_out);
			l_writer.text(field::CONTROL_TRANSFER_ID, p_control.m_transfer_id);
			l_writer.text(field::CONTROL_AGENT_ID, p_control.m_agent_id);
			l_writer.boolean(field::ACCEPTED, p_control.m_accepted);
			l_writer.text(field::REASON, p_control.m_reason);
			return l_out;
		}

		std::vector<std::uint8_t> encode_presence(const Presence& p_presence) {
			std::vector<std::uint8_t> l_out;
			Writer l_writer(l_out);
			l_writer.text(field::NODE_ID, p_presence.m_node_id);
			l_writer.text(field::ENVIRONMENT_ID, p_presence.m_environment_id);
			l_writer.text(field::ENVIRONMENT_NAME, p_presence.m_environment_name);
			l_writer.text(field::NAMESPACE, p_presence.m_namespace);
			l_writer.boolean(field::ONLINE, p_presence.m_online);
			l_writer.boolean(field::HAS_TURN, p_presence.m_turn.has_value());
			l_writer.uint(field::TURN, p_presence.m_turn.value_or(0));
			l_writer.uint(field::TIMESTAMP_MS, p_presence.m_timestamp_ms);
			for (const auto& [l_id, l_name]: p_presence.m_agents) {
				std::vector<std::uint8_t> l_entry;
				Writer l_entry_writer(l_entry);
				l_entry_writer.text(field::ENTRY_ID, l_id);
				l_entry_writer.text(field::ENTRY_NAME, l_name);
				l_writer.message(field::AGENTS, l_entry);
			}
			return l_out;
		}

		std::vector<std::uint8_t> encode_directory(const DirectoryEvent& p_event) {
			std::vector<std::uint8_t> l_out;
			Writer l_writer(l_out);
			l_writer.uint(field::EVENT, static_cast<std::uint64_t>(p_event.m_event));
			l_writer.text(field::EVENT_AGENT_ID, p_event.m_agent_id);
			l_writer.text(field::EVENT_AGENT_NAME, p_event.m_agent_name);
			l_writer.text(field::EVENT_ENVIRONMENT_ID, p_event.m_environment_id);
			l_writer.text(field::PREVIOUS_ENVIRONMENT_ID, p_event.m_previous_environment_id);
			return l_out;
		}

		void write_envelope(Writer& p_writer, const Envelope& p_envelope) {
			p_writer.uint(field::VERSION_MAJOR, p_envelope.m_version_major);
			p_writer.uint(field::VERSION_MINOR, p_envelope.m_version_minor);
			p_writer.uint(field::KIND, static_cast<std::uint64_t>(p_envelope.m_kind));
			p_writer.text(field::SENDER_ENVIRONMENT, p_envelope.m_sender_environment);
			p_writer.text(field::ORIGIN_ENVIRONMENT, p_envelope.m_origin_environment);
			p_writer.uint(field::EPOCH, p_envelope.m_epoch);
			p_writer.uint(field::STREAM_KIND, static_cast<std::uint64_t>(p_envelope.m_stream_kind));
			p_writer.text(field::STREAM_TARGET, p_envelope.m_stream_target);
			p_writer.uint(field::SEQUENCE, p_envelope.m_sequence);
			p_writer.text(field::CONVERSATION_ID, p_envelope.m_conversation_id);
			p_writer.text(field::SENDER_AGENT, p_envelope.m_sender_agent);
			p_writer.uint(field::TARGET_KIND, static_cast<std::uint64_t>(p_envelope.m_target_kind));
			p_writer.text(field::TARGET_ID, p_envelope.m_target_id);
			p_writer.text(field::TARGET_NAME, p_envelope.m_target_name);
			p_writer.boolean(field::FIRST_ONLY, p_envelope.m_first_only);
			p_writer.uint(field::SCOPE, static_cast<std::uint64_t>(p_envelope.m_scope));
			p_writer.uint(field::CONTENT_ENCODING, static_cast<std::uint64_t>(p_envelope.m_content_encoding));
			p_writer.bytes(field::CONTENT, p_envelope.m_content);
			p_writer.uint(field::PRIORITY, p_envelope.m_priority);
			if (const auto* l_transfer = std::get_if<AgentTransfer>(&p_envelope.m_payload)) {
				p_writer.message(field::TRANSFER, encode_transfer(*l_transfer));
			} else if (const auto* l_control = std::get_if<AgentControl>(&p_envelope.m_payload)) {
				p_writer.message(field::CONTROL, encode_control(*l_control));
			} else if (const auto* l_presence = std::get_if<Presence>(&p_envelope.m_payload)) {
				p_writer.message(field::PRESENCE, encode_presence(*l_presence));
			} else if (const auto* l_event = std::get_if<DirectoryEvent>(&p_envelope.m_payload)) {
				p_writer.message(field::DIRECTORY, encode_directory(*l_event));
			}
		}

		// ---------------------------------------------------------------- reading

		Result<Envelope> read_envelope(std::span<const std::uint8_t> p_bytes, std::size_t p_max_bytes, int p_depth);

		Result<AgentTransfer> read_transfer(const std::span<const std::uint8_t> p_bytes, const std::size_t p_max_bytes, const int p_depth) {
			if (p_depth > 0) {
				return invalid("nested agent transfer");
			}
			AgentTransfer l_transfer;
			Reader l_reader(p_bytes);
			std::uint64_t l_field = 0, l_wire = 0;
			while (l_reader.next(l_field, l_wire)) {
				switch (l_field) {
					case field::TRANSFER_ID: l_transfer.m_transfer_id = l_reader.text(l_wire); break;
					case field::TYPE_NAME: l_transfer.m_type_name = l_reader.text(l_wire); break;
					case field::AGENT_ID: l_transfer.m_agent_id = l_reader.text(l_wire); break;
					case field::AGENT_NAME: l_transfer.m_agent_name = l_reader.text(l_wire); break;
					case field::STATE_ENCODING: l_transfer.m_state_encoding = static_cast<MessageBinaryFormat>(enum_byte(l_reader.uint(l_wire))); break;
					case field::STATE: l_transfer.m_state = l_reader.bytes(l_wire, p_max_bytes); break;
					case field::QUEUE: {
						const auto l_nested = l_reader.message(l_wire);
						if (l_reader.error()) break;
						if (l_transfer.m_queue.size() >= ENVELOPE_MAX_LIST_ITEMS) {
							l_reader.fail("too many queued envelopes");
							break;
						}
						auto l_decoded = read_envelope(l_nested, p_max_bytes, p_depth + 1);
						if (!l_decoded) {
							return invalid("invalid queued envelope: " + l_decoded.error().m_message);
						}
						l_transfer.m_queue.push_back(std::move(l_decoded.value()));
						break;
					}
					case field::USES_OBSERVABLES: l_transfer.m_uses_observables = l_reader.uint(l_wire) != 0; break;
					case field::OBSERVABLES: l_transfer.m_observables = l_reader.bytes(l_wire, p_max_bytes); break;
					default: l_reader.skip(l_wire); break;
				}
			}
			if (l_reader.error()) {
				return *l_reader.error();
			}
			return l_transfer;
		}

		Result<AgentControl> read_control(const std::span<const std::uint8_t> p_bytes) {
			AgentControl l_control;
			Reader l_reader(p_bytes);
			std::uint64_t l_field = 0, l_wire = 0;
			while (l_reader.next(l_field, l_wire)) {
				switch (l_field) {
					case field::CONTROL_TRANSFER_ID: l_control.m_transfer_id = l_reader.text(l_wire); break;
					case field::CONTROL_AGENT_ID: l_control.m_agent_id = l_reader.text(l_wire); break;
					case field::ACCEPTED: l_control.m_accepted = l_reader.uint(l_wire) != 0; break;
					case field::REASON: l_control.m_reason = l_reader.text(l_wire); break;
					default: l_reader.skip(l_wire); break;
				}
			}
			if (l_reader.error()) {
				return *l_reader.error();
			}
			return l_control;
		}

		Result<Presence> read_presence(const std::span<const std::uint8_t> p_bytes) {
			Presence l_presence;
			l_presence.m_online = false;  // proto3 default
			bool l_has_turn = false;
			std::uint64_t l_turn = 0;
			Reader l_reader(p_bytes);
			std::uint64_t l_field = 0, l_wire = 0;
			while (l_reader.next(l_field, l_wire)) {
				switch (l_field) {
					case field::NODE_ID: l_presence.m_node_id = l_reader.text(l_wire); break;
					case field::ENVIRONMENT_ID: l_presence.m_environment_id = l_reader.text(l_wire); break;
					case field::ENVIRONMENT_NAME: l_presence.m_environment_name = l_reader.text(l_wire); break;
					case field::NAMESPACE: l_presence.m_namespace = l_reader.text(l_wire); break;
					case field::ONLINE: l_presence.m_online = l_reader.uint(l_wire) != 0; break;
					case field::HAS_TURN: l_has_turn = l_reader.uint(l_wire) != 0; break;
					case field::TURN: l_turn = l_reader.uint(l_wire); break;
					case field::TIMESTAMP_MS: l_presence.m_timestamp_ms = l_reader.uint(l_wire); break;
					case field::AGENTS: {
						const auto l_entry = l_reader.message(l_wire);
						if (l_reader.error()) break;
						if (l_presence.m_agents.size() >= ENVELOPE_MAX_LIST_ITEMS) {
							l_reader.fail("too many agents in presence");
							break;
						}
						std::string l_id, l_name;
						Reader l_entry_reader(l_entry);
						std::uint64_t l_entry_field = 0, l_entry_wire = 0;
						while (l_entry_reader.next(l_entry_field, l_entry_wire)) {
							switch (l_entry_field) {
								case field::ENTRY_ID: l_id = l_entry_reader.text(l_entry_wire); break;
								case field::ENTRY_NAME: l_name = l_entry_reader.text(l_entry_wire); break;
								default: l_entry_reader.skip(l_entry_wire); break;
							}
						}
						if (l_entry_reader.error()) {
							return *l_entry_reader.error();
						}
						l_presence.m_agents.emplace_back(std::move(l_id), std::move(l_name));
						break;
					}
					default: l_reader.skip(l_wire); break;
				}
			}
			if (l_reader.error()) {
				return *l_reader.error();
			}
			if (l_has_turn) {
				l_presence.m_turn = l_turn;
			}
			return l_presence;
		}

		Result<DirectoryEvent> read_directory(const std::span<const std::uint8_t> p_bytes) {
			DirectoryEvent l_event;
			Reader l_reader(p_bytes);
			std::uint64_t l_field = 0, l_wire = 0;
			while (l_reader.next(l_field, l_wire)) {
				switch (l_field) {
					case field::EVENT: l_event.m_event = static_cast<DirectoryEventKind>(enum_byte(l_reader.uint(l_wire))); break;
					case field::EVENT_AGENT_ID: l_event.m_agent_id = l_reader.text(l_wire); break;
					case field::EVENT_AGENT_NAME: l_event.m_agent_name = l_reader.text(l_wire); break;
					case field::EVENT_ENVIRONMENT_ID: l_event.m_environment_id = l_reader.text(l_wire); break;
					case field::PREVIOUS_ENVIRONMENT_ID: l_event.m_previous_environment_id = l_reader.text(l_wire); break;
					default: l_reader.skip(l_wire); break;
				}
			}
			if (l_reader.error()) {
				return *l_reader.error();
			}
			return l_event;
		}

		// One Envelope message (no header). Fields come in any order, the last occurrence wins, unknown
		// fields are skipped; the payload is the last oneof member seen.
		Result<Envelope> read_envelope(const std::span<const std::uint8_t> p_bytes, const std::size_t p_max_bytes, const int p_depth) {
			Envelope l_envelope;
			l_envelope.m_version_major = 0;  // absent means 0 in proto3: refused by the validation
			Reader l_reader(p_bytes);
			std::uint64_t l_field = 0, l_wire = 0;
			while (l_reader.next(l_field, l_wire)) {
				switch (l_field) {
					case field::VERSION_MAJOR: l_envelope.m_version_major = enum_byte(l_reader.uint(l_wire)); break;
					case field::VERSION_MINOR: l_envelope.m_version_minor = enum_byte(l_reader.uint(l_wire)); break;
					case field::KIND: l_envelope.m_kind = static_cast<EnvelopeKind>(enum_byte(l_reader.uint(l_wire))); break;
					case field::SENDER_ENVIRONMENT: l_envelope.m_sender_environment = l_reader.text(l_wire); break;
					case field::ORIGIN_ENVIRONMENT: l_envelope.m_origin_environment = l_reader.text(l_wire); break;
					case field::EPOCH: l_envelope.m_epoch = static_cast<std::uint32_t>(l_reader.uint(l_wire)); break;
					case field::STREAM_KIND: l_envelope.m_stream_kind = static_cast<StreamKind>(enum_byte(l_reader.uint(l_wire))); break;
					case field::STREAM_TARGET: l_envelope.m_stream_target = l_reader.text(l_wire); break;
					case field::SEQUENCE: l_envelope.m_sequence = l_reader.uint(l_wire); break;
					case field::CONVERSATION_ID: l_envelope.m_conversation_id = l_reader.text(l_wire); break;
					case field::SENDER_AGENT: l_envelope.m_sender_agent = l_reader.text(l_wire); break;
					case field::TARGET_KIND: l_envelope.m_target_kind = static_cast<TargetKind>(enum_byte(l_reader.uint(l_wire))); break;
					case field::TARGET_ID: l_envelope.m_target_id = l_reader.text(l_wire); break;
					case field::TARGET_NAME: l_envelope.m_target_name = l_reader.text(l_wire); break;
					case field::FIRST_ONLY: l_envelope.m_first_only = l_reader.uint(l_wire) != 0; break;
					case field::SCOPE: l_envelope.m_scope = static_cast<Scope>(enum_byte(l_reader.uint(l_wire))); break;
					case field::CONTENT_ENCODING: l_envelope.m_content_encoding = static_cast<MessageBinaryFormat>(enum_byte(l_reader.uint(l_wire))); break;
					case field::CONTENT: l_envelope.m_content = l_reader.bytes(l_wire, p_max_bytes); break;
					case field::PRIORITY: l_envelope.m_priority = static_cast<std::uint32_t>(l_reader.uint(l_wire)); break;
					case field::TRANSFER: {
						const auto l_nested = l_reader.message(l_wire);
						if (l_reader.error()) break;
						auto l_transfer = read_transfer(l_nested, p_max_bytes, p_depth);
						if (!l_transfer) return l_transfer.error();
						l_envelope.m_payload = std::move(l_transfer.value());
						break;
					}
					case field::CONTROL: {
						const auto l_nested = l_reader.message(l_wire);
						if (l_reader.error()) break;
						auto l_control = read_control(l_nested);
						if (!l_control) return l_control.error();
						l_envelope.m_payload = std::move(l_control.value());
						break;
					}
					case field::PRESENCE: {
						const auto l_nested = l_reader.message(l_wire);
						if (l_reader.error()) break;
						auto l_presence = read_presence(l_nested);
						if (!l_presence) return l_presence.error();
						l_envelope.m_payload = std::move(l_presence.value());
						break;
					}
					case field::DIRECTORY: {
						const auto l_nested = l_reader.message(l_wire);
						if (l_reader.error()) break;
						auto l_event = read_directory(l_nested);
						if (!l_event) return l_event.error();
						l_envelope.m_payload = std::move(l_event.value());
						break;
					}
					default: l_reader.skip(l_wire); break;
				}
			}
			if (l_reader.error()) {
				return *l_reader.error();
			}
			if (const auto l_valid = validate_envelope(l_envelope, p_max_bytes); !l_valid) {
				return l_valid.error();
			}
			return l_envelope;
		}

	} // namespace

	Result<std::vector<std::uint8_t>> ProtobufCodec::encode(const Envelope& p_envelope) const {
		std::vector<std::uint8_t> l_out;
		l_out.reserve(128 + p_envelope.m_content.size());
		write_header(l_out, ID, VERSION_MAJOR);
		Writer l_writer(l_out);
		write_envelope(l_writer, p_envelope);
		return l_out;
	}

	Result<Envelope> ProtobufCodec::decode(const std::span<const std::uint8_t> p_bytes, const std::size_t p_max_bytes) const {
		if (p_bytes.size() > p_max_bytes) {
			return make_error(ErrorCategory::Validation, ErrorCode::TooLarge, "envelope larger than the configured maximum");
		}
		const auto l_header = read_header(p_bytes);
		if (!l_header) {
			return l_header.error();
		}
		if (l_header.value().m_codec_id != ID) {
			return make_error(ErrorCategory::Validation, ErrorCode::UnsupportedCodec, "unexpected codec id");
		}
		if (l_header.value().m_version_major != VERSION_MAJOR) {
			return make_error(ErrorCategory::Validation, ErrorCode::UnsupportedVersion, "unsupported protobuf codec version");
		}
		return read_envelope(p_bytes.subspan(ENVELOPE_HEADER_SIZE), p_max_bytes, 0);
	}

} // namespace cam::transport
