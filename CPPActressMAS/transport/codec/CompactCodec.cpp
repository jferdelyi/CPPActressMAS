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

#include "CompactCodec.h"

#include <cstring>

namespace cam::transport {

	namespace {

		constexpr std::uint8_t PAYLOAD_NONE = 0;
		constexpr std::uint8_t PAYLOAD_TRANSFER = 1;
		constexpr std::uint8_t PAYLOAD_CONTROL = 2;
		constexpr std::uint8_t PAYLOAD_PRESENCE = 3;
		constexpr std::uint8_t PAYLOAD_DIRECTORY = 4;

		Error invalid(std::string p_message) {
			return make_error(ErrorCategory::Validation, ErrorCode::InvalidEnvelope, std::move(p_message));
		}

		/**
		 * Little-endian writer.
		 **/
		class Writer final {
		private:
			std::vector<std::uint8_t>& m_out;

		public:
			explicit Writer(std::vector<std::uint8_t>& p_out) : m_out(p_out) {}

			void u8(const std::uint8_t p_value) { m_out.push_back(p_value); }

			void u32(const std::uint32_t p_value) {
				for (int i = 0; i < 4; i++) {
					m_out.push_back(static_cast<std::uint8_t>(p_value >> (8 * i)));
				}
			}

			void u64(const std::uint64_t p_value) {
				for (int i = 0; i < 8; i++) {
					m_out.push_back(static_cast<std::uint8_t>(p_value >> (8 * i)));
				}
			}

			void bytes(const std::vector<std::uint8_t>& p_bytes) {
				u32(static_cast<std::uint32_t>(p_bytes.size()));
				m_out.insert(m_out.end(), p_bytes.begin(), p_bytes.end());
			}

			void text(const std::string& p_text) {
				u32(static_cast<std::uint32_t>(p_text.size()));
				m_out.insert(m_out.end(), p_text.begin(), p_text.end());
			}
		};

		/**
		 * Little-endian bounds-checked reader; the first failure is sticky.
		 **/
		class Reader final {
		private:
			std::span<const std::uint8_t> m_in;
			std::size_t m_position = 0;
			std::optional<Error> m_error;

			bool need(const std::size_t p_count) {
				if (m_error) {
					return false;
				}
				if (m_position + p_count > m_in.size()) {
					m_error = invalid("truncated envelope");
					return false;
				}
				return true;
			}

		public:
			explicit Reader(const std::span<const std::uint8_t> p_in, const std::size_t p_offset) : m_in(p_in), m_position(p_offset) {}

			[[nodiscard]] const std::optional<Error>& error() const { return m_error; }
			[[nodiscard]] bool finished() const { return m_position == m_in.size(); }

			std::uint8_t u8() {
				if (!need(1)) return 0;
				return m_in[m_position++];
			}

			std::uint32_t u32() {
				if (!need(4)) return 0;
				std::uint32_t l_value = 0;
				for (int i = 0; i < 4; i++) {
					l_value |= static_cast<std::uint32_t>(m_in[m_position++]) << (8 * i);
				}
				return l_value;
			}

			std::uint64_t u64() {
				if (!need(8)) return 0;
				std::uint64_t l_value = 0;
				for (int i = 0; i < 8; i++) {
					l_value |= static_cast<std::uint64_t>(m_in[m_position++]) << (8 * i);
				}
				return l_value;
			}

			std::vector<std::uint8_t> bytes(const std::size_t p_max) {
				const std::uint32_t l_size = u32();
				if (m_error) return {};
				if (l_size > p_max) {
					m_error = make_error(ErrorCategory::Validation, ErrorCode::TooLarge, "byte array too large");
					return {};
				}
				if (!need(l_size)) return {};
				std::vector<std::uint8_t> l_result(m_in.begin() + static_cast<std::ptrdiff_t>(m_position),
												   m_in.begin() + static_cast<std::ptrdiff_t>(m_position + l_size));
				m_position += l_size;
				return l_result;
			}

			std::string text() {
				const std::uint32_t l_size = u32();
				if (m_error) return {};
				if (l_size > ENVELOPE_MAX_STRING_BYTES) {
					m_error = make_error(ErrorCategory::Validation, ErrorCode::TooLarge, "string too large");
					return {};
				}
				if (!need(l_size)) return {};
				std::string l_result(reinterpret_cast<const char*>(m_in.data() + m_position), l_size);
				m_position += l_size;
				return l_result;
			}

			void fail(std::string p_message) {
				if (!m_error) {
					m_error = invalid(std::move(p_message));
				}
			}
		};

		void write_envelope_body(Writer& p_writer, const Envelope& p_envelope, const CompactCodec& p_codec);

		void write_payload(Writer& p_writer, const Envelope& p_envelope, const CompactCodec& p_codec) {
			if (const auto* l_transfer = std::get_if<AgentTransfer>(&p_envelope.m_payload)) {
				p_writer.u8(PAYLOAD_TRANSFER);
				p_writer.text(l_transfer->m_transfer_id);
				p_writer.text(l_transfer->m_type_name);
				p_writer.text(l_transfer->m_agent_id);
				p_writer.text(l_transfer->m_agent_name);
				p_writer.u8(static_cast<std::uint8_t>(l_transfer->m_state_encoding));
				p_writer.bytes(l_transfer->m_state);
				p_writer.u32(static_cast<std::uint32_t>(l_transfer->m_queue.size()));
				for (const auto& l_queued: l_transfer->m_queue) {
					std::vector<std::uint8_t> l_nested;
					EnvelopeCodec::write_header(l_nested, p_codec.id(), p_codec.version_major());
					Writer l_nested_writer(l_nested);
					write_envelope_body(l_nested_writer, l_queued, p_codec);
					p_writer.bytes(l_nested);
				}
				p_writer.u8(l_transfer->m_uses_observables ? 1 : 0);
				p_writer.bytes(l_transfer->m_observables);
			} else if (const auto* l_control = std::get_if<AgentControl>(&p_envelope.m_payload)) {
				p_writer.u8(PAYLOAD_CONTROL);
				p_writer.text(l_control->m_transfer_id);
				p_writer.text(l_control->m_agent_id);
				p_writer.u8(l_control->m_accepted ? 1 : 0);
				p_writer.text(l_control->m_reason);
			} else if (const auto* l_presence = std::get_if<Presence>(&p_envelope.m_payload)) {
				p_writer.u8(PAYLOAD_PRESENCE);
				p_writer.text(l_presence->m_node_id);
				p_writer.text(l_presence->m_environment_id);
				p_writer.text(l_presence->m_environment_name);
				p_writer.text(l_presence->m_namespace);
				p_writer.u8(l_presence->m_online ? 1 : 0);
				p_writer.u8(l_presence->m_turn.has_value() ? 1 : 0);
				p_writer.u64(l_presence->m_turn.value_or(0));
				p_writer.u64(l_presence->m_timestamp_ms);
				p_writer.u32(static_cast<std::uint32_t>(l_presence->m_agents.size()));
				for (const auto& [l_id, l_name]: l_presence->m_agents) {
					p_writer.text(l_id);
					p_writer.text(l_name);
				}
			} else if (const auto* l_event = std::get_if<DirectoryEvent>(&p_envelope.m_payload)) {
				p_writer.u8(PAYLOAD_DIRECTORY);
				p_writer.u8(static_cast<std::uint8_t>(l_event->m_event));
				p_writer.text(l_event->m_agent_id);
				p_writer.text(l_event->m_agent_name);
				p_writer.text(l_event->m_environment_id);
				p_writer.text(l_event->m_previous_environment_id);
			} else {
				p_writer.u8(PAYLOAD_NONE);
			}
		}

		void write_envelope_body(Writer& p_writer, const Envelope& p_envelope, const CompactCodec& p_codec) {
			p_writer.u8(p_envelope.m_version_minor);
			p_writer.u8(static_cast<std::uint8_t>(p_envelope.m_kind));
			p_writer.text(p_envelope.m_sender_environment);
			p_writer.text(p_envelope.m_origin_environment);
			p_writer.u32(p_envelope.m_epoch);
			p_writer.u8(static_cast<std::uint8_t>(p_envelope.m_stream_kind));
			p_writer.text(p_envelope.m_stream_target);
			p_writer.u64(p_envelope.m_sequence);
			p_writer.text(p_envelope.m_conversation_id);
			p_writer.text(p_envelope.m_sender_agent);
			if (p_envelope.m_version_minor >= 1) {
				p_writer.text(p_envelope.m_sender_name);  // since 1.1
			}
			p_writer.u8(static_cast<std::uint8_t>(p_envelope.m_target_kind));
			p_writer.text(p_envelope.m_target_id);
			p_writer.text(p_envelope.m_target_name);
			p_writer.u8(p_envelope.m_first_only ? 1 : 0);
			p_writer.u8(static_cast<std::uint8_t>(p_envelope.m_scope));
			p_writer.u8(static_cast<std::uint8_t>(p_envelope.m_content_encoding));
			p_writer.bytes(p_envelope.m_content);
			p_writer.u32(p_envelope.m_priority);
			write_payload(p_writer, p_envelope, p_codec);
		}

		Result<Envelope> read_envelope(std::span<const std::uint8_t> p_bytes, std::size_t p_max_bytes, const CompactCodec& p_codec, int p_depth);

		void read_payload(Reader& p_reader, Envelope& p_envelope, const std::size_t p_max_bytes, const CompactCodec& p_codec, const int p_depth) {
			const std::uint8_t l_tag = p_reader.u8();
			switch (l_tag) {
				case PAYLOAD_NONE:
					p_envelope.m_payload = std::monostate{};
					break;
				case PAYLOAD_TRANSFER: {
					AgentTransfer l_transfer;
					l_transfer.m_transfer_id = p_reader.text();
					l_transfer.m_type_name = p_reader.text();
					l_transfer.m_agent_id = p_reader.text();
					l_transfer.m_agent_name = p_reader.text();
					l_transfer.m_state_encoding = static_cast<MessageBinaryFormat>(p_reader.u8());
					l_transfer.m_state = p_reader.bytes(p_max_bytes);
					const std::uint32_t l_count = p_reader.u32();
					if (l_count > ENVELOPE_MAX_LIST_ITEMS) {
						p_reader.fail("too many queued envelopes");
						return;
					}
					if (p_depth > 0) {
						p_reader.fail("nested agent transfer");
						return;
					}
					for (std::uint32_t i = 0; i < l_count && !p_reader.error(); i++) {
						const auto l_nested = p_reader.bytes(p_max_bytes);
						if (p_reader.error()) break;
						auto l_decoded = read_envelope(l_nested, p_max_bytes, p_codec, p_depth + 1);
						if (!l_decoded) {
							p_reader.fail("invalid queued envelope: " + l_decoded.error().m_message);
							return;
						}
						l_transfer.m_queue.push_back(std::move(l_decoded.value()));
					}
					l_transfer.m_uses_observables = p_reader.u8() != 0;
					l_transfer.m_observables = p_reader.bytes(p_max_bytes);
					p_envelope.m_payload = std::move(l_transfer);
					break;
				}
				case PAYLOAD_CONTROL: {
					AgentControl l_control;
					l_control.m_transfer_id = p_reader.text();
					l_control.m_agent_id = p_reader.text();
					l_control.m_accepted = p_reader.u8() != 0;
					l_control.m_reason = p_reader.text();
					p_envelope.m_payload = std::move(l_control);
					break;
				}
				case PAYLOAD_PRESENCE: {
					Presence l_presence;
					l_presence.m_node_id = p_reader.text();
					l_presence.m_environment_id = p_reader.text();
					l_presence.m_environment_name = p_reader.text();
					l_presence.m_namespace = p_reader.text();
					l_presence.m_online = p_reader.u8() != 0;
					const bool l_has_turn = p_reader.u8() != 0;
					const std::uint64_t l_turn = p_reader.u64();
					if (l_has_turn) l_presence.m_turn = l_turn;
					l_presence.m_timestamp_ms = p_reader.u64();
					const std::uint32_t l_count = p_reader.u32();
					if (l_count > ENVELOPE_MAX_LIST_ITEMS) {
						p_reader.fail("too many agents in presence");
						return;
					}
					for (std::uint32_t i = 0; i < l_count && !p_reader.error(); i++) {
						std::string l_id = p_reader.text();
						std::string l_name = p_reader.text();
						l_presence.m_agents.emplace_back(std::move(l_id), std::move(l_name));
					}
					p_envelope.m_payload = std::move(l_presence);
					break;
				}
				case PAYLOAD_DIRECTORY: {
					DirectoryEvent l_event;
					l_event.m_event = static_cast<DirectoryEventKind>(p_reader.u8());
					l_event.m_agent_id = p_reader.text();
					l_event.m_agent_name = p_reader.text();
					l_event.m_environment_id = p_reader.text();
					l_event.m_previous_environment_id = p_reader.text();
					p_envelope.m_payload = std::move(l_event);
					break;
				}
				default:
					p_reader.fail("unknown payload tag");
					break;
			}
		}

		Result<Envelope> read_envelope(const std::span<const std::uint8_t> p_bytes, const std::size_t p_max_bytes,
									   const CompactCodec& p_codec, const int p_depth) {
			const auto l_header = EnvelopeCodec::read_header(p_bytes);
			if (!l_header) {
				return l_header.error();
			}
			if (l_header.value().m_codec_id != p_codec.id()) {
				return make_error(ErrorCategory::Validation, ErrorCode::UnsupportedCodec, "unexpected codec id");
			}
			if (l_header.value().m_version_major != p_codec.version_major()) {
				return make_error(ErrorCategory::Validation, ErrorCode::UnsupportedVersion, "unsupported compact codec version");
			}
			Reader l_reader(p_bytes, ENVELOPE_HEADER_SIZE);
			Envelope l_envelope;
			l_envelope.m_version_major = l_header.value().m_version_major;
			l_envelope.m_version_minor = l_reader.u8();
			l_envelope.m_kind = static_cast<EnvelopeKind>(l_reader.u8());
			l_envelope.m_sender_environment = l_reader.text();
			l_envelope.m_origin_environment = l_reader.text();
			l_envelope.m_epoch = l_reader.u32();
			l_envelope.m_stream_kind = static_cast<StreamKind>(l_reader.u8());
			l_envelope.m_stream_target = l_reader.text();
			l_envelope.m_sequence = l_reader.u64();
			l_envelope.m_conversation_id = l_reader.text();
			l_envelope.m_sender_agent = l_reader.text();
			if (l_envelope.m_version_minor >= 1) {
				l_envelope.m_sender_name = l_reader.text();  // since 1.1
			}
			l_envelope.m_target_kind = static_cast<TargetKind>(l_reader.u8());
			l_envelope.m_target_id = l_reader.text();
			l_envelope.m_target_name = l_reader.text();
			l_envelope.m_first_only = l_reader.u8() != 0;
			l_envelope.m_scope = static_cast<Scope>(l_reader.u8());
			l_envelope.m_content_encoding = static_cast<MessageBinaryFormat>(l_reader.u8());
			l_envelope.m_content = l_reader.bytes(p_max_bytes);
			l_envelope.m_priority = l_reader.u32();
			read_payload(l_reader, l_envelope, p_max_bytes, p_codec, p_depth);
			if (l_reader.error()) {
				return *l_reader.error();
			}
			if (!l_reader.finished()) {
				return invalid("trailing bytes after envelope");
			}
			if (const auto l_valid = validate_envelope(l_envelope, p_max_bytes); !l_valid) {
				return l_valid.error();
			}
			return l_envelope;
		}

	} // namespace

	Result<std::vector<std::uint8_t>> CompactCodec::encode(const Envelope& p_envelope) const {
		std::vector<std::uint8_t> l_out;
		l_out.reserve(128 + p_envelope.m_content.size());
		write_header(l_out, ID, VERSION_MAJOR);
		Writer l_writer(l_out);
		write_envelope_body(l_writer, p_envelope, *this);
		return l_out;
	}

	Result<Envelope> CompactCodec::decode(const std::span<const std::uint8_t> p_bytes, const std::size_t p_max_bytes) const {
		if (p_bytes.size() > p_max_bytes) {
			return make_error(ErrorCategory::Validation, ErrorCode::TooLarge, "envelope larger than the configured maximum");
		}
		return read_envelope(p_bytes, p_max_bytes, *this, 0);
	}

} // namespace cam::transport
