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

#include "ActressMasTransport.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <random>

#include "../../util/Base64.h"
#include "../../util/Identifier.h"
#include "../codec/ContentCodec.h"
#include "../codec/ProtobufCodec.h"

namespace cam::transport {

	namespace {

		// A text that starts with it carries bytes, as their base64
		constexpr std::string_view BYTES_PREFIX = "base64:";

		std::uint64_t now_ms() {
			return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
					std::chrono::system_clock::now().time_since_epoch()).count());
		}

		std::uint32_t draw_epoch() {
			std::random_device l_device;
			const std::uint32_t l_epoch = l_device();
			return l_epoch == 0 ? 1 : l_epoch;
		}

		// FNV-1a over the bytes of the text, then a SplitMix64 finish: the same value in every process and platform
		std::uint64_t hash(const std::string_view p_text, std::uint64_t p_hash) {
			for (const char l_char: p_text) {
				p_hash ^= static_cast<unsigned char>(l_char);
				p_hash *= 1099511628211ull;
			}
			p_hash += 0x9e3779b97f4a7c15ull;
			p_hash = (p_hash ^ (p_hash >> 30)) * 0xbf58476d1ce4e5b9ull;
			p_hash = (p_hash ^ (p_hash >> 27)) * 0x94d049bb133111ebull;
			return p_hash ^ (p_hash >> 31);
		}

		// Two independent hashes of a text
		std::array<std::uint64_t, 2> hashes(const std::string_view p_text) {
			return {hash(p_text, 14695981039346656037ull), hash(p_text, 0x84222325cbf29ce4ull)};
		}

		// A canonical UUID of version 8 (RFC 9562, custom) from a text
		std::string uuid_of(const std::string_view p_text) {
			const auto l_hashes = hashes(p_text);
			std::array<std::uint8_t, 16> l_bytes{};
			for (std::size_t i = 0; i < 8; i++) {
				l_bytes[i] = static_cast<std::uint8_t>(l_hashes[0] >> (56 - 8 * i));
				l_bytes[8 + i] = static_cast<std::uint8_t>(l_hashes[1] >> (56 - 8 * i));
			}
			l_bytes[6] = static_cast<std::uint8_t>((l_bytes[6] & 0x0F) | 0x80);
			l_bytes[8] = static_cast<std::uint8_t>((l_bytes[8] & 0x3F) | 0x80);
			static constexpr std::string_view s_digits = "0123456789abcdef";
			std::string l_uuid;
			l_uuid.reserve(36);
			for (std::size_t i = 0; i < l_bytes.size(); i++) {
				if (i == 4 || i == 6 || i == 8 || i == 10) {
					l_uuid += '-';
				}
				l_uuid += s_digits[l_bytes[i] >> 4];
				l_uuid += s_digits[l_bytes[i] & 0x0F];
			}
			return l_uuid;
		}

		// A container name the server can list: not empty, no space (the list separator), no '@' (the address separator)
		bool is_valid_container(const std::string_view p_name) {
			return !p_name.empty() && p_name.size() <= 128 &&
				   std::none_of(p_name.begin(), p_name.end(), [](const char p_char) {
					   return p_char == '@' || static_cast<unsigned char>(p_char) <= ' ';
				   });
		}

		// "host:port"
		bool split_endpoint(const std::string& p_endpoint, std::string& p_host, int& p_port) {
			const std::size_t l_colon = p_endpoint.rfind(':');
			if (l_colon == std::string::npos || l_colon == 0 || l_colon + 1 >= p_endpoint.size()) {
				return false;
			}
			const std::string l_port = p_endpoint.substr(l_colon + 1);
			if (!std::all_of(l_port.begin(), l_port.end(), [](const char p_char) { return p_char >= '0' && p_char <= '9'; }) ||
				l_port.size() > 5) {
				return false;
			}
			p_host = p_endpoint.substr(0, l_colon);
			p_port = std::stoi(l_port);
			return p_port > 0 && p_port < 65536;
		}

		Error not_supported(const std::string& p_message) {
			return make_error(ErrorCategory::Routing, ErrorCode::NotSupported, p_message);
		}

	} // namespace

	ActressMasTransport::ActressMasTransport(TransportConfig p_config, ActressMasOptions p_options, std::shared_ptr<const EnvelopeCodec> p_codec) :
			m_config(std::move(p_config)),
			m_options(std::move(p_options)),
			m_codec(p_codec ? std::move(p_codec) : std::make_shared<ProtobufCodec>()),
			m_node_id(cam::generate_uuid()),
			m_epoch(draw_epoch()) {
	}

	ActressMasTransport::~ActressMasTransport() {
		if (m_open.load()) {
			(void) close(std::chrono::milliseconds(2000));
		} else if (m_reader.joinable()) {
			shutdown(std::chrono::milliseconds(2000));
		}
	}

	// ---------------------------------------------------------------- identifiers and contents

	std::string ActressMasTransport::environment_id_of(const std::string_view p_container) {
		return uuid_of("actressmas environment " + std::string(p_container));
	}

	std::string ActressMasTransport::node_id_of(const std::string_view p_container) {
		return uuid_of("actressmas node " + std::string(p_container));
	}

	std::string ActressMasTransport::agent_id_of(const std::string_view p_address) {
		// 8 base62 digits from the first hash and 7 from the second: about 89 bits, as a generated identifier
		static constexpr std::string_view s_digits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
		auto l_hashes = hashes("actressmas agent " + std::string(p_address));
		std::string l_id;
		l_id.reserve(AGENT_ID_LENGTH);
		for (std::size_t i = 0; i < AGENT_ID_LENGTH; i++) {
			std::uint64_t& l_hash = l_hashes[i < 8 ? 0 : 1];
			l_id += s_digits[l_hash % 62];
			l_hash /= 62;
		}
		return l_id;
	}

	std::optional<std::pair<std::string, std::string>> ActressMasTransport::split_address(const std::string_view p_address) {
		const std::size_t l_at = p_address.rfind('@');
		if (l_at == std::string_view::npos || l_at == 0 || l_at + 1 >= p_address.size()) {
			return std::nullopt;
		}
		return std::make_pair(std::string(p_address.substr(0, l_at)), std::string(p_address.substr(l_at + 1)));
	}

	Result<std::string> ActressMasTransport::text_of(const Envelope& p_envelope) {
		const auto l_content = decode_content(p_envelope.m_content, p_envelope.m_content_encoding);
		if (!l_content) {
			return l_content.error();
		}
		const MessageContent& l_value = l_content.value();
		if (const auto* l_text = std::get_if<std::string>(&l_value)) {
			return *l_text;
		}
		if (const auto* l_text = std::get_if<Text>(&l_value)) {
			return l_text->str();
		}
		if (const auto* l_json = std::get_if<std::shared_ptr<const json>>(&l_value)) {
			return *l_json ? (*l_json)->dump() : std::string("null");
		}
		if (const auto* l_bytes = std::get_if<std::vector<std::uint8_t>>(&l_value)) {
			return std::string(BYTES_PREFIX) + cam::to_base64(*l_bytes);
		}
		return std::string();
	}

	void ActressMasTransport::set_text(Envelope& p_envelope, const std::string& p_text) {
		if (std::string_view(p_text).starts_with(BYTES_PREFIX)) {
			if (auto l_bytes = cam::from_base64(std::string_view(p_text).substr(BYTES_PREFIX.size()))) {
				if (auto l_encoded = encode_content(MessageContent(std::move(*l_bytes)), ContentEncoding::Raw)) {
					p_envelope.m_content_encoding = ContentEncoding::Raw;
					p_envelope.m_content = std::move(l_encoded.value());
					return;
				}
			}
		}
		if (auto l_encoded = encode_content(MessageContent(p_text), ContentEncoding::Text)) {
			p_envelope.m_content_encoding = ContentEncoding::Text;
			p_envelope.m_content = std::move(l_encoded.value());
		}
	}

	// ---------------------------------------------------------------- connection

	Result<void> ActressMasTransport::send_frame(const ContainerMessage& p_message) {
		const auto l_bytes = ActressMasCodec::encode_frame(p_message);
		const std::lock_guard<std::mutex> l_lock(m_write_mutex);
		return m_connection.send_all(l_bytes);
	}

	void ActressMasTransport::read_frames() {
		std::vector<std::uint8_t> l_frame(ActressMasCodec::HEADER_SIZE);
		while (m_connection.receive(std::span<std::uint8_t>(l_frame.data(), ActressMasCodec::HEADER_SIZE))) {
			const auto l_size = ActressMasCodec::read_frame_size(l_frame, m_config.m_max_envelope_bytes);
			if (!l_size) {
				// Without a valid header the next frame cannot be found: the connection is given up
				++m_malformed;
				break;
			}
			l_frame.resize(l_size.value());
			if (!m_connection.receive(std::span<std::uint8_t>(l_frame.data() + ActressMasCodec::HEADER_SIZE,
															  l_size.value() - ActressMasCodec::HEADER_SIZE))) {
				break;
			}
			if (const auto l_message = ActressMasCodec::decode_frame(l_frame, m_config.m_max_envelope_bytes); l_message) {
				handle(l_message.value());
			} else {
				// Its length is known: the next frame is still readable
				++m_malformed;
			}
			l_frame.resize(ActressMasCodec::HEADER_SIZE);
		}
		const std::lock_guard<std::mutex> l_lock(m_mutex);
		m_reader_done = true;
		if (m_open.load()) {
			// Lost while open: the other containers are out of reach, and the protocol has no reconnection
			for (const auto& l_container: m_containers) {
				deliver_presence(l_container, false);
			}
			m_containers.clear();
			m_registry.for_each(nullptr, [](Endpoint& p_endpoint) { p_endpoint.push_event(TransportEvent::Disconnected); });
		}
		m_condition.notify_all();
	}

	void ActressMasTransport::handle(const ContainerMessage& p_message) {
		if (p_message.m_info == ActressMasCodec::INFORM_CONTAINERS) {
			on_container_list(p_message.m_content);
		} else if (p_message.m_info == ActressMasCodec::SEND_REMOTE_MESSAGE) {
			const auto l_remote = ActressMasCodec::decode_remote_message(p_message.m_content);
			if (!l_remote) {
				++m_malformed;
				return;
			}
			on_remote_message(p_message.m_sender, l_remote.value());
		} else if (p_message.m_info == ActressMasCodec::INFORM_INVALID_NAME) {
			const std::lock_guard<std::mutex> l_lock(m_mutex);
			m_refused = true;
			m_condition.notify_all();
		} else if (p_message.m_info == ActressMasCodec::REQUEST_MOVE_AGENT) {
			// An agent of the C# is a .NET object: it cannot run in this process
			++m_refused_moves;
			m_registry.for_each(nullptr, [](Endpoint& p_endpoint) { p_endpoint.count_rejected(); });
		} else {
			++m_malformed;
		}
	}

	void ActressMasTransport::on_container_list(const std::string_view p_list) {
		std::vector<std::string> l_others;
		bool l_listed = false;
		std::size_t l_start = 0;
		while (l_start < p_list.size()) {
			std::size_t l_space = p_list.find(' ', l_start);
			if (l_space == std::string_view::npos) {
				l_space = p_list.size();
			}
			const std::string l_name(p_list.substr(l_start, l_space - l_start));
			if (l_name == m_options.m_container) {
				l_listed = true;
			} else if (!l_name.empty() && std::find(l_others.begin(), l_others.end(), l_name) == l_others.end()) {
				l_others.push_back(l_name);
			}
			l_start = l_space + 1;
		}
		const std::lock_guard<std::mutex> l_lock(m_mutex);
		for (const auto& l_container: m_containers) {
			if (std::find(l_others.begin(), l_others.end(), l_container) == l_others.end()) {
				deliver_presence(l_container, false);
			}
		}
		for (const auto& l_container: l_others) {
			if (std::find(m_containers.begin(), m_containers.end(), l_container) == m_containers.end()) {
				deliver_presence(l_container, true);
			}
		}
		m_containers = std::move(l_others);
		if (l_listed && !m_registered) {
			m_registered = true;
			m_condition.notify_all();
		}
	}

	void ActressMasTransport::on_remote_message(const std::string& p_container, const RemoteMessage& p_message) {
		// The sender is "name@container" as the C# writes it; a bare name belongs to the container of the frame
		std::string l_sender = p_message.m_sender;
		std::string l_container = p_container;
		if (const auto l_address = split_address(l_sender)) {
			l_container = l_address->second;
		} else {
			l_sender += "@" + p_container;
		}
		std::string l_target = p_message.m_receiver;
		if (const auto l_address = split_address(l_target)) {
			l_target = l_address->first;
		}
		const std::lock_guard<std::mutex> l_lock(m_mutex);
		if (m_environment_id.empty()) {
			return;
		}
		if (l_container != m_options.m_container &&
			std::find(m_containers.begin(), m_containers.end(), l_container) == m_containers.end()) {
			// A message may come before the next list of the server
			m_containers.push_back(l_container);
			deliver_presence(l_container, true);
		}
		const std::string l_container_id = environment_id_of(l_container);
		const std::string l_agent_id = agent_id_of(l_sender);
		if (m_agent_addresses.emplace(l_agent_id, l_sender).second) {
			// The directory learns where the sender is, so that a reply by identifier goes back to it
			Envelope l_event;
			l_event.m_version_minor = ENVELOPE_VERSION_MINOR;
			l_event.m_kind = EnvelopeKind::Directory;
			l_event.m_sender_environment = l_container_id;
			l_event.m_epoch = m_epoch;
			l_event.m_stream_kind = StreamKind::Presence;
			l_event.m_stream_target = l_container_id;
			l_event.m_sequence = next_sequence("presence " + l_container);
			DirectoryEvent l_added;
			l_added.m_event = DirectoryEventKind::AgentAdded;
			l_added.m_agent_id = l_agent_id;
			l_added.m_agent_name = l_sender;
			l_added.m_environment_id = l_container_id;
			l_event.m_payload = std::move(l_added);
			deliver(Address{AddressKind::EnvPresence, m_config.m_namespace, l_container_id}, std::move(l_event));
		}
		Envelope l_message;
		l_message.m_version_minor = ENVELOPE_VERSION_MINOR;
		l_message.m_kind = EnvelopeKind::Message;
		l_message.m_sender_environment = l_container_id;
		l_message.m_epoch = m_epoch;
		l_message.m_stream_kind = StreamKind::In;
		l_message.m_stream_target = m_environment_id;
		l_message.m_sequence = next_sequence("in " + l_container);
		l_message.m_conversation_id = p_message.m_conversation_id;
		l_message.m_sender_agent = l_agent_id;
		l_message.m_sender_name = l_sender;
		l_message.m_target_kind = TargetKind::Name;
		l_message.m_target_name = l_target;
		l_message.m_first_only = true;  // names are unique in a container of the C#
		l_message.m_scope = Scope::Network;
		set_text(l_message, p_message.m_content);
		deliver(Address{AddressKind::EnvIn, m_config.m_namespace, m_environment_id}, std::move(l_message));
	}

	void ActressMasTransport::deliver_presence(const std::string& p_container, const bool p_online) {
		const std::string l_id = environment_id_of(p_container);
		Envelope l_envelope;
		l_envelope.m_version_minor = ENVELOPE_VERSION_MINOR;
		l_envelope.m_kind = EnvelopeKind::Presence;
		l_envelope.m_sender_environment = l_id;
		l_envelope.m_epoch = m_epoch;
		l_envelope.m_stream_kind = StreamKind::Presence;
		l_envelope.m_stream_target = l_id;
		l_envelope.m_sequence = next_sequence("presence " + p_container);
		Presence l_presence;
		l_presence.m_node_id = node_id_of(p_container);
		l_presence.m_environment_id = l_id;
		l_presence.m_environment_name = p_container;
		l_presence.m_namespace = m_config.m_namespace;
		l_presence.m_online = p_online;
		l_presence.m_timestamp_ms = now_ms();
		l_envelope.m_payload = std::move(l_presence);
		const Address l_address{AddressKind::EnvPresence, m_config.m_namespace, l_id};
		m_presence.remember(l_address, l_envelope);
		deliver(l_address, std::move(l_envelope));
	}

	void ActressMasTransport::deliver(const Address& p_address, Envelope&& p_envelope) {
		// One environment at most: the envelope goes to it as it is
		m_registry.for_each(nullptr, [&](Endpoint& p_endpoint) { p_endpoint.deliver(p_address, std::move(p_envelope)); });
	}

	std::uint64_t ActressMasTransport::next_sequence(const std::string& p_flow) {
		return ++m_sequences[p_flow];
	}

	void ActressMasTransport::shutdown(const std::chrono::milliseconds p_timeout) {
		m_connection.shutdown();
		{
			std::unique_lock<std::mutex> l_lock(m_mutex);
			(void) m_condition.wait_for(l_lock, p_timeout, [&] { return m_reader_done; });
		}
		if (m_reader.joinable()) {
			m_reader.join();
		}
		const std::lock_guard<std::mutex> l_lock(m_write_mutex);
		m_connection.close();
	}

	// ---------------------------------------------------------------- Transport

	Result<void> ActressMasTransport::open() {
		if (m_open.load()) {
			return make_error(ErrorCategory::State, ErrorCode::AlreadyConnected, "actressmas transport already open");
		}
		if (!is_valid_container(m_options.m_container)) {
			return make_error(ErrorCategory::State, ErrorCode::InvalidConfiguration, "invalid container name: \"" + m_options.m_container + "\"");
		}
		std::string l_host;
		int l_port = 0;
		if (!split_endpoint(m_config.m_endpoint, l_host, l_port)) {
			return make_error(ErrorCategory::State, ErrorCode::InvalidConfiguration, "no server address");
		}
		{
			const std::lock_guard<std::mutex> l_lock(m_mutex);
			m_registered = false;
			m_refused = false;
			m_reader_done = false;
			m_containers.clear();
		}
		if (const auto l_result = m_connection.connect(l_host, l_port, m_config.m_connect_timeout); !l_result) {
			return l_result;
		}
		m_reader = std::thread(&ActressMasTransport::read_frames, this);
		const ContainerMessage l_register{m_options.m_container, std::string(ActressMasCodec::SERVER), std::string(ActressMasCodec::REQUEST_REGISTER), ""};
		if (const auto l_result = send_frame(l_register); !l_result) {
			shutdown(m_config.m_connect_timeout);
			return l_result;
		}
		// The server says nothing on a registration it accepts: the container shows in its next list
		bool l_answered = false;
		bool l_refused = false;
		{
			std::unique_lock<std::mutex> l_lock(m_mutex);
			l_answered = m_condition.wait_for(l_lock, m_config.m_connect_timeout, [&] { return m_registered || m_refused || m_reader_done; });
			if (m_registered) {
				m_open.store(true);
				return {};
			}
			l_refused = m_refused;
		}
		shutdown(m_config.m_connect_timeout);
		if (l_refused) {
			return make_error(ErrorCategory::State, ErrorCode::InvalidConfiguration,
							  "the server refuses the container name " + m_options.m_container + " (already used)");
		}
		if (!l_answered) {
			return make_error(ErrorCategory::Transport, ErrorCode::Timeout, "the server " + m_config.m_endpoint + " did not list the container " +
																			  m_options.m_container);
		}
		return make_error(ErrorCategory::State, ErrorCode::NotConnected, "the server " + m_config.m_endpoint + " closed the connection");
	}

	Result<void> ActressMasTransport::close(const std::chrono::milliseconds p_timeout) {
		if (!m_open.load()) {
			return make_error(ErrorCategory::State, ErrorCode::NotConnected, "actressmas transport not open");
		}
		m_open.store(false);
		shutdown(p_timeout);
		return {};
	}

	Result<void> ActressMasTransport::attach(Endpoint& p_endpoint) {
		if (!m_open.load()) {
			return make_error(ErrorCategory::State, ErrorCode::NotConnected, "actressmas transport not open");
		}
		{
			const std::lock_guard<std::mutex> l_lock(m_mutex);
			if (!m_environment_id.empty()) {
				return not_supported("one environment per ActressMAS connection: the container " + m_options.m_container + " has one");
			}
			m_environment_id = p_endpoint.environment_id();
		}
		if (!m_registry.add(p_endpoint)) {
			const std::lock_guard<std::mutex> l_lock(m_mutex);
			m_environment_id.clear();
			return make_error(ErrorCategory::State, ErrorCode::AlreadyConnected, "endpoint already attached");
		}
		m_presence.replay(p_endpoint);
		p_endpoint.push_event(TransportEvent::Connected);
		return {};
	}

	Result<void> ActressMasTransport::detach(Endpoint& p_endpoint) {
		if (!m_registry.remove(p_endpoint)) {
			return make_error(ErrorCategory::State, ErrorCode::NotAttached, "endpoint not attached");
		}
		const std::lock_guard<std::mutex> l_lock(m_mutex);
		m_environment_id.clear();
		return {};
	}

	Result<void> ActressMasTransport::publish(const Endpoint&, const Address& p_address, const Envelope& p_envelope) {
		if (!is_connected()) {
			return make_error(ErrorCategory::State, ErrorCode::NotConnected, "actressmas transport not connected");
		}
		if (p_address.m_namespace != m_config.m_namespace) {
			return make_error(ErrorCategory::Routing, ErrorCode::InvalidTarget, "address outside the transport namespace");
		}
		switch (p_envelope.m_kind) {
			case EnvelopeKind::Presence:
			case EnvelopeKind::Directory:
				return {};  // the server keeps the list of the containers
			case EnvelopeKind::Message:
				break;
			default:
				return not_supported("migration is not part of the ActressMAS protocol");
		}
		std::vector<std::string> l_receivers;
		switch (p_envelope.m_target_kind) {
			case TargetKind::Name:
				if (split_address(p_envelope.m_target_name)) {
					l_receivers.push_back(p_envelope.m_target_name);
				} else {
					const std::lock_guard<std::mutex> l_lock(m_mutex);
					for (const auto& l_container: m_containers) {
						l_receivers.push_back(p_envelope.m_target_name + "@" + l_container);
					}
				}
				if (l_receivers.empty()) {
					return make_error(ErrorCategory::Routing, ErrorCode::UnknownDestination, "no other container on the server");
				}
				break;
			case TargetKind::Id: {
				const std::lock_guard<std::mutex> l_lock(m_mutex);
				const auto l_found = m_agent_addresses.find(p_envelope.m_target_id);
				if (l_found == m_agent_addresses.end()) {
					return make_error(ErrorCategory::Routing, ErrorCode::UnknownDestination,
									  "unknown agent " + p_envelope.m_target_id + " (no message came from it)");
				}
				l_receivers.push_back(l_found->second);
				break;
			}
			case TargetKind::Fragment:
			case TargetKind::Broadcast:
			case TargetKind::Environment:
			default:
				return not_supported("broadcast and fragments of names are not part of the ActressMAS protocol");
		}
		const auto l_text = text_of(p_envelope);
		if (!l_text) {
			return l_text.error();
		}
		RemoteMessage l_remote;
		l_remote.m_sender = (p_envelope.m_sender_name.empty() ? p_envelope.m_sender_agent : p_envelope.m_sender_name) + "@" + m_options.m_container;
		l_remote.m_content = l_text.value();
		l_remote.m_conversation_id = p_envelope.m_conversation_id;
		for (const auto& l_receiver: l_receivers) {
			l_remote.m_receiver = l_receiver;
			const ContainerMessage l_frame{m_options.m_container, split_address(l_receiver)->second, std::string(ActressMasCodec::SEND_REMOTE_MESSAGE),
										   ActressMasCodec::encode_remote_message(l_remote)};
			if (const auto l_result = send_frame(l_frame); !l_result) {
				return l_result;
			}
		}
		return {};
	}

	Capabilities ActressMasTransport::capabilities() const {
		Capabilities l_capabilities;
		l_capabilities.m_pubsub = false;
		l_capabilities.m_request_reply = false;
		l_capabilities.m_liveliness = false;
		l_capabilities.m_retained_presence = false;
		l_capabilities.m_ordered_per_sender = false;
		l_capabilities.m_at_least_once = false;
		l_capabilities.m_thread_safe_publish = true;
		return l_capabilities;
	}

	std::vector<std::string> ActressMasTransport::containers() const {
		const std::lock_guard<std::mutex> l_lock(m_mutex);
		return m_containers;
	}

	bool ActressMasTransport::is_connected() const {
		const std::lock_guard<std::mutex> l_lock(m_mutex);
		return m_open.load() && !m_reader_done;
	}

} // namespace cam::transport
