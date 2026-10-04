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

#include "CompactCodec.h"

#include <cstring>

namespace cam::transport {

	namespace {

		constexpr std::size_t MAX_STRING_BYTES = 65535;
		constexpr std::size_t MAX_LIST_ITEMS = 65535;
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
				if (l_size > MAX_STRING_BYTES) {
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
					if (l_count > MAX_LIST_ITEMS) {
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
					if (l_count > MAX_LIST_ITEMS) {
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

		Result<Envelope> read_envelope(const std::span<const std::uint8_t> p_bytes, const std::size_t p_max_bytes, const CompactCodec& p_codec, const int p_depth) {
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

	// ---------------------------------------------------------------- header and validation (EnvelopeCodec.h)

	Result<EnvelopeHeader> EnvelopeCodec::read_header(const std::span<const std::uint8_t> p_bytes) {
		if (p_bytes.size() < ENVELOPE_HEADER_SIZE) {
			return invalid("envelope shorter than its header");
		}
		if (p_bytes[0] != ENVELOPE_MAGIC) {
			return invalid("bad envelope magic");
		}
		return EnvelopeHeader{p_bytes[1], p_bytes[2]};
	}

	void EnvelopeCodec::write_header(std::vector<std::uint8_t>& p_out, const std::uint8_t p_codec_id, const std::uint8_t p_version_major) {
		p_out.push_back(ENVELOPE_MAGIC);
		p_out.push_back(p_codec_id);
		p_out.push_back(p_version_major);
		p_out.push_back(0);
	}

	Result<void> validate_envelope(const Envelope& p_envelope, const std::size_t p_max_content_bytes) {
		if (p_envelope.m_version_major != 1) {
			return make_error(ErrorCategory::Validation, ErrorCode::UnsupportedVersion, "unsupported envelope version");
		}
		if (static_cast<std::uint8_t>(p_envelope.m_kind) > static_cast<std::uint8_t>(EnvelopeKind::Reply)) {
			return invalid("unknown envelope kind");
		}
		if (static_cast<std::uint8_t>(p_envelope.m_stream_kind) > static_cast<std::uint8_t>(StreamKind::All)) {
			return invalid("unknown stream kind");
		}
		if (static_cast<std::uint8_t>(p_envelope.m_target_kind) > static_cast<std::uint8_t>(TargetKind::Environment)) {
			return invalid("unknown target kind");
		}
		if (static_cast<std::uint8_t>(p_envelope.m_scope) > static_cast<std::uint8_t>(Scope::Network)) {
			return invalid("unknown scope");
		}
		if (static_cast<std::uint8_t>(p_envelope.m_content_encoding) > static_cast<std::uint8_t>(MessageBinaryFormat::UBJSON)) {
			return invalid("unknown content encoding");
		}
		if (!is_valid_identifier(p_envelope.m_sender_environment)) {
			return invalid("invalid sender environment");
		}
		if (!p_envelope.m_origin_environment.empty() && !is_valid_identifier(p_envelope.m_origin_environment)) {
			return invalid("invalid origin environment");
		}
		if (p_envelope.m_stream_kind == StreamKind::All) {
			if (!p_envelope.m_stream_target.empty()) {
				return invalid("stream target must be empty on the all stream");
			}
		} else if (!is_valid_identifier(p_envelope.m_stream_target)) {
			return invalid("invalid stream target");
		}
		if (p_envelope.m_content.size() > p_max_content_bytes) {
			return make_error(ErrorCategory::Validation, ErrorCode::TooLarge, "content larger than the configured maximum");
		}
		if (p_envelope.m_kind == EnvelopeKind::Message) {
			if (!is_valid_identifier(p_envelope.m_sender_agent)) {
				return invalid("invalid sender agent");
			}
			switch (p_envelope.m_target_kind) {
				case TargetKind::Id:
					if (!is_valid_identifier(p_envelope.m_target_id)) return invalid("invalid target id");
					break;
				case TargetKind::Name:
				case TargetKind::Fragment:
					if (p_envelope.m_target_name.empty() || p_envelope.m_target_name.size() > MAX_STRING_BYTES) return invalid("invalid target name");
					break;
				case TargetKind::Environment:
					if (!is_valid_identifier(p_envelope.m_target_id)) return invalid("invalid target environment");
					break;
				case TargetKind::Broadcast:
				default:
					break;
			}
			if (!std::holds_alternative<std::monostate>(p_envelope.m_payload)) {
				return invalid("a message carries no typed payload");
			}
		} else if (p_envelope.m_kind == EnvelopeKind::AgentOffer) {
			const auto* l_transfer = std::get_if<AgentTransfer>(&p_envelope.m_payload);
			if (!l_transfer) return invalid("agent offer without transfer payload");
			if (!is_valid_identifier(l_transfer->m_transfer_id) || !is_valid_identifier(l_transfer->m_agent_id) ||
				l_transfer->m_type_name.empty() || l_transfer->m_type_name.size() > MAX_STRING_BYTES) {
				return invalid("invalid agent transfer");
			}
			if (l_transfer->m_state.size() > p_max_content_bytes || l_transfer->m_observables.size() > p_max_content_bytes) {
				return make_error(ErrorCategory::Validation, ErrorCode::TooLarge, "agent state larger than the configured maximum");
			}
		} else if (p_envelope.m_kind == EnvelopeKind::AgentAccept || p_envelope.m_kind == EnvelopeKind::AgentReject ||
				   p_envelope.m_kind == EnvelopeKind::AgentCommit || p_envelope.m_kind == EnvelopeKind::AgentAbort) {
			const auto* l_control = std::get_if<AgentControl>(&p_envelope.m_payload);
			if (!l_control) return invalid("agent control without control payload");
			if (!is_valid_identifier(l_control->m_transfer_id) || !is_valid_identifier(l_control->m_agent_id)) {
				return invalid("invalid agent control");
			}
		} else if (p_envelope.m_kind == EnvelopeKind::Presence) {
			const auto* l_presence = std::get_if<Presence>(&p_envelope.m_payload);
			if (!l_presence) return invalid("presence without presence payload");
			if (!is_valid_identifier(l_presence->m_node_id) || !is_valid_identifier(l_presence->m_environment_id) ||
				!is_valid_namespace(l_presence->m_namespace)) {
				return invalid("invalid presence");
			}
			for (const auto& [l_id, l_name]: l_presence->m_agents) {
				if (!is_valid_identifier(l_id)) return invalid("invalid agent id in presence");
			}
		} else if (p_envelope.m_kind == EnvelopeKind::Directory) {
			const auto* l_event = std::get_if<DirectoryEvent>(&p_envelope.m_payload);
			if (!l_event) return invalid("directory event without payload");
			if (static_cast<std::uint8_t>(l_event->m_event) > static_cast<std::uint8_t>(DirectoryEventKind::AgentMoved) ||
				!is_valid_identifier(l_event->m_agent_id) || !is_valid_identifier(l_event->m_environment_id)) {
				return invalid("invalid directory event");
			}
		}
		return {};
	}

	// ---------------------------------------------------------------- identifiers (Types.h)

	bool is_valid_namespace(const std::string_view p_namespace) {
		if (p_namespace.empty() || p_namespace.size() > 32) {
			return false;
		}
		for (const char l_char: p_namespace) {
			const bool l_ok = (l_char >= 'a' && l_char <= 'z') || (l_char >= '0' && l_char <= '9') || l_char == '-';
			if (!l_ok) return false;
		}
		return true;
	}

	bool is_valid_identifier(const std::string_view p_identifier) {
		if (p_identifier.empty() || p_identifier.size() > 128) {
			return false;
		}
		for (const char l_char: p_identifier) {
			if (l_char <= 0x20 || l_char >= 0x7f || l_char == '/' || l_char == '+' || l_char == '#' || l_char == '*') {
				return false;
			}
		}
		return true;
	}

	bool is_canonical_uuid(const std::string_view p_text) {
		if (p_text.size() != 36) {
			return false;
		}
		for (std::size_t i = 0; i < 36; i++) {
			const char l_char = p_text[i];
			if (i == 8 || i == 13 || i == 18 || i == 23) {
				if (l_char != '-') return false;
			} else {
				const bool l_hex = (l_char >= '0' && l_char <= '9') || (l_char >= 'a' && l_char <= 'f') || (l_char >= 'A' && l_char <= 'F');
				if (!l_hex) return false;
			}
		}
		return true;
	}

	const char* to_string(const ErrorCode p_code) {
		switch (p_code) {
			case ErrorCode::None: return "none";
			case ErrorCode::NotConnected: return "not connected";
			case ErrorCode::AlreadyConnected: return "already connected";
			case ErrorCode::NotAttached: return "not attached";
			case ErrorCode::UnknownDestination: return "unknown destination";
			case ErrorCode::TooLarge: return "too large";
			case ErrorCode::OutboxFull: return "outbox full";
			case ErrorCode::InboxFull: return "inbox full";
			case ErrorCode::InvalidTarget: return "invalid target";
			case ErrorCode::InvalidEnvelope: return "invalid envelope";
			case ErrorCode::UnsupportedCodec: return "unsupported codec";
			case ErrorCode::UnsupportedVersion: return "unsupported version";
			case ErrorCode::Timeout: return "timeout";
			case ErrorCode::TransportFailure: return "transport failure";
			case ErrorCode::UnknownAgentType: return "unknown agent type";
			case ErrorCode::InvalidConfiguration: return "invalid configuration";
			case ErrorCode::NotSupported: return "not supported";
			case ErrorCode::InTransit: return "agent in transit";
			default: return "unknown";
		}
	}

	// ---------------------------------------------------------------- addresses (Types.h)

	std::string Address::to_key() const {
		std::string l_key = "cam/v1/" + m_namespace;
		switch (m_kind) {
			case AddressKind::EnvIn: return l_key + "/env/" + m_target + "/in";
			case AddressKind::EnvAgent: return l_key + "/env/" + m_target + "/agent";
			case AddressKind::EnvQuery: return l_key + "/env/" + m_target + "/query";
			case AddressKind::EnvPresence: return l_key + "/env/" + m_target + "/presence";
			case AddressKind::NodePresence: return l_key + "/node/" + m_target + "/presence";
			case AddressKind::AllIn:
			default: return l_key + "/all/in";
		}
	}

	std::optional<Address> Address::parse(const std::string_view p_key) {
		std::vector<std::string_view> l_levels;
		std::size_t l_start = 0;
		while (l_start <= p_key.size()) {
			const std::size_t l_slash = p_key.find('/', l_start);
			const std::size_t l_end = l_slash == std::string_view::npos ? p_key.size() : l_slash;
			l_levels.push_back(p_key.substr(l_start, l_end - l_start));
			if (l_slash == std::string_view::npos) break;
			l_start = l_slash + 1;
		}
		if (l_levels.size() < 5 || l_levels[0] != "cam" || l_levels[1] != "v1" || !is_valid_namespace(l_levels[2])) {
			return std::nullopt;
		}
		Address l_address;
		l_address.m_namespace = std::string(l_levels[2]);
		if (l_levels[3] == "all" && l_levels.size() == 5 && l_levels[4] == "in") {
			l_address.m_kind = AddressKind::AllIn;
			return l_address;
		}
		if (l_levels.size() != 6 || !is_valid_identifier(l_levels[4])) {
			return std::nullopt;
		}
		l_address.m_target = std::string(l_levels[4]);
		if (l_levels[3] == "node") {
			if (l_levels[5] != "presence") return std::nullopt;
			l_address.m_kind = AddressKind::NodePresence;
			return l_address;
		}
		if (l_levels[3] != "env") {
			return std::nullopt;
		}
		if (l_levels[5] == "in") l_address.m_kind = AddressKind::EnvIn;
		else if (l_levels[5] == "agent") l_address.m_kind = AddressKind::EnvAgent;
		else if (l_levels[5] == "query") l_address.m_kind = AddressKind::EnvQuery;
		else if (l_levels[5] == "presence") l_address.m_kind = AddressKind::EnvPresence;
		else return std::nullopt;
		return l_address;
	}

} // namespace cam::transport
