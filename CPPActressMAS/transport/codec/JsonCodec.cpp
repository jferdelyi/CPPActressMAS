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

#include "JsonCodec.h"

#include <array>

#include <nlohmann/json.hpp>

#include "../../util/Base64.h"

namespace cam::transport {

	namespace {

		using json = nlohmann::json;

		Error invalid(std::string p_message) {
			return make_error(ErrorCategory::Validation, ErrorCode::InvalidEnvelope, std::move(p_message));
		}

		// ---------------------------------------------------------------- enums as strings (names of envelope.proto)

		constexpr std::array KINDS = {"message", "agent_offer", "agent_accept", "agent_reject", "agent_commit",
									  "agent_abort", "presence", "directory", "query", "reply"};
		constexpr std::array STREAMS = {"in", "agent", "query", "presence", "all"};
		constexpr std::array TARGETS = {"id", "name", "fragment", "broadcast", "environment"};
		constexpr std::array SCOPES = {"local", "network"};
		constexpr std::array ENCODINGS = {"raw", "bjdata", "bson", "cbor", "messagepack", "ubjson", "text", "json"};
		constexpr std::array EVENTS = {"agent_added", "agent_removed", "agent_moved"};

		template<std::size_t N>
		std::string enum_name(const std::array<const char*, N>& p_names, const std::uint8_t p_value) {
			return p_value < N ? p_names[p_value] : std::to_string(p_value);  // out of range: the number, refused on reading
		}

		// An unknown name becomes 255, out of every range: the validation refuses it.
		template<std::size_t N>
		std::uint8_t enum_value(const std::array<const char*, N>& p_names, const json& p_json, const char* p_what, std::optional<Error>& p_error) {
			if (!p_json.is_string()) {
				if (!p_error) p_error = invalid(std::string(p_what) + " is not a string");
				return 255;
			}
			const auto& l_text = p_json.get_ref<const std::string&>();
			for (std::size_t i = 0; i < N; i++) {
				if (l_text == p_names[i]) return static_cast<std::uint8_t>(i);
			}
			return 255;
		}

		// ---------------------------------------------------------------- writing

		json envelope_to_json(const Envelope& p_envelope);

		json payload_to_json(const Envelope& p_envelope) {
			if (const auto* l_transfer = std::get_if<AgentTransfer>(&p_envelope.m_payload)) {
				json l_queue = json::array();
				for (const auto& l_queued: l_transfer->m_queue) {
					l_queue.push_back(envelope_to_json(l_queued));
				}
				return {{"transfer_id", l_transfer->m_transfer_id}, {"type_name", l_transfer->m_type_name},
						{"agent_id", l_transfer->m_agent_id}, {"agent_name", l_transfer->m_agent_name},
						{"state_encoding", enum_name(ENCODINGS, static_cast<std::uint8_t>(l_transfer->m_state_encoding))},
						{"state", to_base64(l_transfer->m_state)}, {"queue", l_queue},
						{"uses_observables", l_transfer->m_uses_observables}, {"observables", to_base64(l_transfer->m_observables)}};
			}
			if (const auto* l_control = std::get_if<AgentControl>(&p_envelope.m_payload)) {
				return {{"transfer_id", l_control->m_transfer_id}, {"agent_id", l_control->m_agent_id},
						{"accepted", l_control->m_accepted}, {"reason", l_control->m_reason}};
			}
			if (const auto* l_presence = std::get_if<Presence>(&p_envelope.m_payload)) {
				json l_agents = json::array();
				for (const auto& [l_id, l_name]: l_presence->m_agents) {
					l_agents.push_back({{"id", l_id}, {"name", l_name}});
				}
				json l_result = {{"node_id", l_presence->m_node_id}, {"environment_id", l_presence->m_environment_id},
								 {"environment_name", l_presence->m_environment_name}, {"namespace", l_presence->m_namespace},
								 {"online", l_presence->m_online}, {"timestamp_ms", l_presence->m_timestamp_ms}, {"agents", l_agents}};
				if (l_presence->m_turn) {
					l_result["turn"] = *l_presence->m_turn;
				}
				return l_result;
			}
			if (const auto* l_event = std::get_if<DirectoryEvent>(&p_envelope.m_payload)) {
				return {{"event", enum_name(EVENTS, static_cast<std::uint8_t>(l_event->m_event))}, {"agent_id", l_event->m_agent_id},
						{"agent_name", l_event->m_agent_name}, {"environment_id", l_event->m_environment_id},
						{"previous_environment_id", l_event->m_previous_environment_id}};
			}
			return nullptr;
		}

		json envelope_to_json(const Envelope& p_envelope) {
			json l_json = {{"version_major", p_envelope.m_version_major},
						   {"version_minor", p_envelope.m_version_minor},
						   {"kind", enum_name(KINDS, static_cast<std::uint8_t>(p_envelope.m_kind))},
						   {"sender_environment", p_envelope.m_sender_environment},
						   {"origin_environment", p_envelope.m_origin_environment},
						   {"epoch", p_envelope.m_epoch},
						   {"stream_kind", enum_name(STREAMS, static_cast<std::uint8_t>(p_envelope.m_stream_kind))},
						   {"stream_target", p_envelope.m_stream_target},
						   {"sequence", p_envelope.m_sequence},
						   {"conversation_id", p_envelope.m_conversation_id},
						   {"sender_agent", p_envelope.m_sender_agent},
						   {"target_kind", enum_name(TARGETS, static_cast<std::uint8_t>(p_envelope.m_target_kind))},
						   {"target_id", p_envelope.m_target_id},
						   {"target_name", p_envelope.m_target_name},
						   {"first_only", p_envelope.m_first_only},
						   {"scope", enum_name(SCOPES, static_cast<std::uint8_t>(p_envelope.m_scope))},
						   {"content_encoding", enum_name(ENCODINGS, static_cast<std::uint8_t>(p_envelope.m_content_encoding))},
						   {"priority", p_envelope.m_priority}};
			// The name of the sender only when known (since 1.1): an envelope 1.0 reads the same
			if (!p_envelope.m_sender_name.empty()) {
				l_json["sender_name"] = p_envelope.m_sender_name;
			}
			// A text content is readable as is; anything else travels in base64.
			if (p_envelope.m_content_encoding == ContentEncoding::Text || p_envelope.m_content_encoding == ContentEncoding::Json) {
				l_json["content_text"] = std::string(p_envelope.m_content.begin(), p_envelope.m_content.end());
			} else {
				l_json["content"] = to_base64(p_envelope.m_content);
			}
			if (const auto* l_transfer = std::get_if<AgentTransfer>(&p_envelope.m_payload)) {
				l_json["transfer"] = payload_to_json(p_envelope);
				(void) l_transfer;
			} else if (std::holds_alternative<AgentControl>(p_envelope.m_payload)) {
				l_json["control"] = payload_to_json(p_envelope);
			} else if (std::holds_alternative<Presence>(p_envelope.m_payload)) {
				l_json["presence"] = payload_to_json(p_envelope);
			} else if (std::holds_alternative<DirectoryEvent>(p_envelope.m_payload)) {
				l_json["directory"] = payload_to_json(p_envelope);
			}
			return l_json;
		}

		// ---------------------------------------------------------------- reading (tolerant: absent = default, unknown ignored)

		class Reader final {
		private:
			std::optional<Error> m_error;
			std::size_t m_max_bytes;

		public:
			explicit Reader(const std::size_t p_max_bytes) : m_max_bytes(p_max_bytes) {}

			[[nodiscard]] const std::optional<Error>& error() const { return m_error; }

			void fail(std::string p_message) {
				if (!m_error) m_error = invalid(std::move(p_message));
			}

			std::string text(const json& p_object, const char* p_key) {
				const auto l_it = p_object.find(p_key);
				if (l_it == p_object.end() || l_it->is_null()) return {};
				if (!l_it->is_string()) {
					fail(std::string(p_key) + " is not a string");
					return {};
				}
				const auto& l_text = l_it->get_ref<const std::string&>();
				if (l_text.size() > ENVELOPE_MAX_STRING_BYTES) {
					if (!m_error) m_error = make_error(ErrorCategory::Validation, ErrorCode::TooLarge, "string too large");
					return {};
				}
				return l_text;
			}

			std::uint64_t uint(const json& p_object, const char* p_key) {
				const auto l_it = p_object.find(p_key);
				if (l_it == p_object.end() || l_it->is_null()) return 0;
				if (!l_it->is_number_unsigned()) {
					fail(std::string(p_key) + " is not an unsigned integer");
					return 0;
				}
				return l_it->get<std::uint64_t>();
			}

			std::uint8_t byte(const json& p_object, const char* p_key) {
				const std::uint64_t l_value = uint(p_object, p_key);
				return l_value > 255 ? static_cast<std::uint8_t>(255) : static_cast<std::uint8_t>(l_value);
			}

			bool boolean(const json& p_object, const char* p_key) {
				const auto l_it = p_object.find(p_key);
				if (l_it == p_object.end() || l_it->is_null()) return false;
				if (!l_it->is_boolean()) {
					fail(std::string(p_key) + " is not a boolean");
					return false;
				}
				return l_it->get<bool>();
			}

			template<std::size_t N>
			std::uint8_t enumeration(const json& p_object, const char* p_key, const std::array<const char*, N>& p_names) {
				const auto l_it = p_object.find(p_key);
				if (l_it == p_object.end() || l_it->is_null()) return 0;
				return enum_value(p_names, *l_it, p_key, m_error);
			}

			std::vector<std::uint8_t> bytes(const json& p_object, const char* p_key) {
				const auto l_it = p_object.find(p_key);
				if (l_it == p_object.end() || l_it->is_null()) return {};
				if (!l_it->is_string()) {
					fail(std::string(p_key) + " is not a base64 string");
					return {};
				}
				auto l_bytes = from_base64(l_it->get_ref<const std::string&>());
				if (!l_bytes) {
					fail(std::string(p_key) + " is not valid base64");
					return {};
				}
				if (l_bytes->size() > m_max_bytes) {
					if (!m_error) m_error = make_error(ErrorCategory::Validation, ErrorCode::TooLarge, "byte array too large");
					return {};
				}
				return std::move(*l_bytes);
			}

			const json* object(const json& p_object, const char* p_key) {
				const auto l_it = p_object.find(p_key);
				if (l_it == p_object.end() || l_it->is_null()) return nullptr;
				if (!l_it->is_object()) {
					fail(std::string(p_key) + " is not an object");
					return nullptr;
				}
				return &*l_it;
			}
		};

		Result<Envelope> envelope_from_json(const json& p_json, std::size_t p_max_bytes, int p_depth);

		Result<Envelope> envelope_from_json(const json& p_json, const std::size_t p_max_bytes, const int p_depth) {
			if (!p_json.is_object()) {
				return invalid("envelope is not a JSON object");
			}
			Reader l_reader(p_max_bytes);
			Envelope l_envelope;
			l_envelope.m_version_major = l_reader.byte(p_json, "version_major");
			l_envelope.m_version_minor = l_reader.byte(p_json, "version_minor");
			l_envelope.m_kind = static_cast<EnvelopeKind>(l_reader.enumeration(p_json, "kind", KINDS));
			l_envelope.m_sender_environment = l_reader.text(p_json, "sender_environment");
			l_envelope.m_origin_environment = l_reader.text(p_json, "origin_environment");
			l_envelope.m_epoch = static_cast<std::uint32_t>(l_reader.uint(p_json, "epoch"));
			l_envelope.m_stream_kind = static_cast<StreamKind>(l_reader.enumeration(p_json, "stream_kind", STREAMS));
			l_envelope.m_stream_target = l_reader.text(p_json, "stream_target");
			l_envelope.m_sequence = l_reader.uint(p_json, "sequence");
			l_envelope.m_conversation_id = l_reader.text(p_json, "conversation_id");
			l_envelope.m_sender_agent = l_reader.text(p_json, "sender_agent");
			l_envelope.m_sender_name = l_reader.text(p_json, "sender_name");
			l_envelope.m_target_kind = static_cast<TargetKind>(l_reader.enumeration(p_json, "target_kind", TARGETS));
			l_envelope.m_target_id = l_reader.text(p_json, "target_id");
			l_envelope.m_target_name = l_reader.text(p_json, "target_name");
			l_envelope.m_first_only = l_reader.boolean(p_json, "first_only");
			l_envelope.m_scope = static_cast<Scope>(l_reader.enumeration(p_json, "scope", SCOPES));
			l_envelope.m_content_encoding = static_cast<ContentEncoding>(l_reader.enumeration(p_json, "content_encoding", ENCODINGS));
			l_envelope.m_priority = static_cast<std::uint32_t>(l_reader.uint(p_json, "priority"));
			if (p_json.contains("content_text")) {
				const auto l_it = p_json.find("content_text");
				if (!l_it->is_string()) {
					l_reader.fail("content_text is not a string");
				} else {
					const auto& l_text = l_it->get_ref<const std::string&>();
					if (l_text.size() > p_max_bytes) {
						return make_error(ErrorCategory::Validation, ErrorCode::TooLarge, "content larger than the configured maximum");
					}
					l_envelope.m_content.assign(l_text.begin(), l_text.end());
				}
			} else {
				l_envelope.m_content = l_reader.bytes(p_json, "content");
			}
			if (const json* l_object = l_reader.object(p_json, "transfer")) {
				if (p_depth > 0) {
					return invalid("nested agent transfer");
				}
				AgentTransfer l_transfer;
				l_transfer.m_transfer_id = l_reader.text(*l_object, "transfer_id");
				l_transfer.m_type_name = l_reader.text(*l_object, "type_name");
				l_transfer.m_agent_id = l_reader.text(*l_object, "agent_id");
				l_transfer.m_agent_name = l_reader.text(*l_object, "agent_name");
				l_transfer.m_state_encoding = static_cast<ContentEncoding>(l_reader.enumeration(*l_object, "state_encoding", ENCODINGS));
				l_transfer.m_state = l_reader.bytes(*l_object, "state");
				if (const auto l_queue = l_object->find("queue"); l_queue != l_object->end() && !l_queue->is_null()) {
					if (!l_queue->is_array()) {
						return invalid("queue is not an array");
					}
					if (l_queue->size() > ENVELOPE_MAX_LIST_ITEMS) {
						return invalid("too many queued envelopes");
					}
					for (const auto& l_queued: *l_queue) {
						auto l_decoded = envelope_from_json(l_queued, p_max_bytes, p_depth + 1);
						if (!l_decoded) {
							return invalid("invalid queued envelope: " + l_decoded.error().m_message);
						}
						l_transfer.m_queue.push_back(std::move(l_decoded.value()));
					}
				}
				l_transfer.m_uses_observables = l_reader.boolean(*l_object, "uses_observables");
				l_transfer.m_observables = l_reader.bytes(*l_object, "observables");
				l_envelope.m_payload = std::move(l_transfer);
			} else if (const json* l_control_object = l_reader.object(p_json, "control")) {
				AgentControl l_control;
				l_control.m_transfer_id = l_reader.text(*l_control_object, "transfer_id");
				l_control.m_agent_id = l_reader.text(*l_control_object, "agent_id");
				l_control.m_accepted = l_reader.boolean(*l_control_object, "accepted");
				l_control.m_reason = l_reader.text(*l_control_object, "reason");
				l_envelope.m_payload = std::move(l_control);
			} else if (const json* l_presence_object = l_reader.object(p_json, "presence")) {
				Presence l_presence;
				l_presence.m_node_id = l_reader.text(*l_presence_object, "node_id");
				l_presence.m_environment_id = l_reader.text(*l_presence_object, "environment_id");
				l_presence.m_environment_name = l_reader.text(*l_presence_object, "environment_name");
				l_presence.m_namespace = l_reader.text(*l_presence_object, "namespace");
				l_presence.m_online = l_reader.boolean(*l_presence_object, "online");
				if (l_presence_object->contains("turn") && !l_presence_object->at("turn").is_null()) {
					l_presence.m_turn = l_reader.uint(*l_presence_object, "turn");
				}
				l_presence.m_timestamp_ms = l_reader.uint(*l_presence_object, "timestamp_ms");
				if (const auto l_agents = l_presence_object->find("agents"); l_agents != l_presence_object->end() && !l_agents->is_null()) {
					if (!l_agents->is_array()) {
						return invalid("agents is not an array");
					}
					if (l_agents->size() > ENVELOPE_MAX_LIST_ITEMS) {
						return invalid("too many agents in presence");
					}
					for (const auto& l_agent: *l_agents) {
						if (!l_agent.is_object()) {
							return invalid("agent entry is not an object");
						}
						l_presence.m_agents.emplace_back(l_reader.text(l_agent, "id"), l_reader.text(l_agent, "name"));
					}
				}
				l_envelope.m_payload = std::move(l_presence);
			} else if (const json* l_event_object = l_reader.object(p_json, "directory")) {
				DirectoryEvent l_event;
				l_event.m_event = static_cast<DirectoryEventKind>(l_reader.enumeration(*l_event_object, "event", EVENTS));
				l_event.m_agent_id = l_reader.text(*l_event_object, "agent_id");
				l_event.m_agent_name = l_reader.text(*l_event_object, "agent_name");
				l_event.m_environment_id = l_reader.text(*l_event_object, "environment_id");
				l_event.m_previous_environment_id = l_reader.text(*l_event_object, "previous_environment_id");
				l_envelope.m_payload = std::move(l_event);
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

	Result<std::vector<std::uint8_t>> JsonCodec::encode(const Envelope& p_envelope) const {
		std::vector<std::uint8_t> l_out;
		write_header(l_out, ID, VERSION_MAJOR);
		const std::string l_text = envelope_to_json(p_envelope).dump();
		l_out.insert(l_out.end(), l_text.begin(), l_text.end());
		return l_out;
	}

	Result<Envelope> JsonCodec::decode(const std::span<const std::uint8_t> p_bytes, const std::size_t p_max_bytes) const {
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
			return make_error(ErrorCategory::Validation, ErrorCode::UnsupportedVersion, "unsupported json codec version");
		}
		json l_json;
		try {
			l_json = json::parse(p_bytes.begin() + ENVELOPE_HEADER_SIZE, p_bytes.end());
		} catch (const json::exception& p_exception) {
			return invalid(std::string("envelope is not valid JSON: ") + p_exception.what());
		}
		return envelope_from_json(l_json, p_max_bytes, 0);
	}

} // namespace cam::transport
