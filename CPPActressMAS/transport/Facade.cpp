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

#include "Facade.h"

#include <map>
#include <mutex>

#include "codec/EnvelopeCodec.h"
#include "loopback/LoopbackTransport.h"
#if CAM_TRANSPORT_MQTT
#include "mqtt/MqttTransport.h"
#endif
#if CAM_TRANSPORT_ZENOH
#include "zenoh/ZenohTransport.h"
#endif

namespace cam::transport {

	namespace {

		Error invalid_url(const std::string& p_message) {
			return make_error(ErrorCategory::State, ErrorCode::InvalidConfiguration, p_message);
		}

		// key=value pairs separated by '&'
		std::vector<std::pair<std::string, std::string>> parse_query(const std::string& p_query) {
			std::vector<std::pair<std::string, std::string>> l_pairs;
			std::size_t l_start = 0;
			while (l_start < p_query.size()) {
				const std::size_t l_amp = p_query.find('&', l_start);
				const std::string l_item = p_query.substr(l_start, l_amp == std::string::npos ? std::string::npos : l_amp - l_start);
				const std::size_t l_equal = l_item.find('=');
				if (l_equal == std::string::npos) {
					l_pairs.emplace_back(l_item, "");
				} else {
					l_pairs.emplace_back(l_item.substr(0, l_equal), l_item.substr(l_equal + 1));
				}
				if (l_amp == std::string::npos) break;
				l_start = l_amp + 1;
			}
			return l_pairs;
		}

#if CAM_TRANSPORT_ZENOH
		// values separated by ',' (only the Zenoh endpoints are lists)
		std::vector<std::string> split_list(const std::string& p_text) {
			std::vector<std::string> l_values;
			std::size_t l_start = 0;
			while (l_start <= p_text.size()) {
				const std::size_t l_comma = p_text.find(',', l_start);
				const std::string l_value = p_text.substr(l_start, l_comma == std::string::npos ? std::string::npos : l_comma - l_start);
				if (!l_value.empty()) l_values.push_back(l_value);
				if (l_comma == std::string::npos) break;
				l_start = l_comma + 1;
			}
			return l_values;
		}
#endif

		bool is_mqtt(const std::string& p_scheme) {
			return p_scheme == "mqtt" || p_scheme == "mqtts";
		}

	} // namespace

	std::vector<std::string> schemes() {
		std::vector<std::string> l_schemes = {"loopback"};
#if CAM_TRANSPORT_MQTT
		l_schemes.emplace_back("mqtt");
		l_schemes.emplace_back("mqtts");
#endif
#if CAM_TRANSPORT_ZENOH
		l_schemes.emplace_back("zenoh");
#endif
		return l_schemes;
	}

	Result<ParsedUrl> parse_url(const std::string& p_url, TransportConfig p_config) {
		const std::size_t l_separator = p_url.find("://");
		if (l_separator == std::string::npos || l_separator == 0) {
			return invalid_url("transport URL without scheme: " + p_url);
		}
		ParsedUrl l_parsed;
		l_parsed.m_scheme = p_url.substr(0, l_separator);
		std::string l_rest = p_url.substr(l_separator + 3);
		std::string l_query;
		if (const std::size_t l_question = l_rest.find('?'); l_question != std::string::npos) {
			l_query = l_rest.substr(l_question + 1);
			l_rest = l_rest.substr(0, l_question);
		}
		std::string l_path;
		if (const std::size_t l_slash = l_rest.find('/'); l_slash != std::string::npos) {
			l_path = l_rest.substr(l_slash + 1);
			l_rest = l_rest.substr(0, l_slash);
		}
		const std::string l_authority = l_rest;
		if (!l_authority.empty()) {
			l_parsed.m_host = l_authority;
			if (const std::size_t l_colon = l_authority.rfind(':'); l_colon != std::string::npos && l_authority.find(']') == std::string::npos) {
				l_parsed.m_host = l_authority.substr(0, l_colon);
				l_parsed.m_port = l_authority.substr(l_colon + 1);
			}
		}

		if (l_parsed.m_scheme == "loopback") {
			if (!l_authority.empty()) {
				p_config.m_namespace = l_authority;
			}
		} else if (is_mqtt(l_parsed.m_scheme)) {
			if (l_authority.empty()) {
				return invalid_url("mqtt URL without host: " + p_url);
			}
			if (l_parsed.m_port.empty()) {
				l_parsed.m_port = l_parsed.m_scheme == "mqtts" ? "8883" : "1883";
			}
			p_config.m_tls = l_parsed.m_scheme == "mqtts";
			p_config.m_endpoint = (p_config.m_tls ? "ssl://" : "tcp://") + l_parsed.m_host + ":" + l_parsed.m_port;
			if (!l_path.empty()) {
				p_config.m_namespace = l_path;
			}
		} else if (l_parsed.m_scheme == "zenoh") {
			if (!l_authority.empty()) {
				if (l_parsed.m_port.empty()) {
					l_parsed.m_port = "7447";
				}
				p_config.m_endpoint = "tcp/" + l_parsed.m_host + ":" + l_parsed.m_port;
			}
			if (!l_path.empty()) {
				p_config.m_namespace = l_path;
			}
		} else {
			return make_error(ErrorCategory::State, ErrorCode::NotSupported, "unknown transport scheme: " + l_parsed.m_scheme);
		}

		for (const auto& [l_key, l_value]: parse_query(l_query)) {
			if (l_key == "ns" || l_key == "namespace") p_config.m_namespace = l_value;
			else if (l_key == "codec") p_config.m_codec = l_value;
			else if (is_mqtt(l_parsed.m_scheme) && l_key == "user") p_config.m_username = l_value;
			else if (is_mqtt(l_parsed.m_scheme) && l_key == "password") p_config.m_password = l_value;
			else if (is_mqtt(l_parsed.m_scheme) && l_key == "ca") p_config.m_ca_file = l_value;
			else if (is_mqtt(l_parsed.m_scheme) && l_key == "cert") p_config.m_certificate_file = l_value;
			else if (is_mqtt(l_parsed.m_scheme) && l_key == "key") p_config.m_key_file = l_value;
			else if (is_mqtt(l_parsed.m_scheme) && l_key == "verify") p_config.m_verify_peer = l_value != "0" && l_value != "false";
			else if (is_mqtt(l_parsed.m_scheme) && l_key == "mqtt") {
				if (l_value != "5" && l_value != "3.1.1") return invalid_url("unknown MQTT version: " + l_value);
				l_parsed.m_options.emplace_back(l_key, l_value);
			} else if (l_parsed.m_scheme == "zenoh" && (l_key == "mode" || l_key == "listen" || l_key == "connect" || l_key == "scouting")) {
				if (l_key == "mode" && l_value != "peer" && l_value != "client") return invalid_url("unknown zenoh mode: " + l_value);
				if (l_key == "scouting" && l_value != "multicast") return invalid_url("unknown zenoh scouting: " + l_value);
				l_parsed.m_options.emplace_back(l_key, l_value);
			} else {
				return invalid_url("unknown transport option: " + l_key);
			}
		}
		if (!is_valid_namespace(p_config.m_namespace)) {
			return invalid_url("invalid namespace: " + p_config.m_namespace);
		}
		if (!make_codec(p_config.m_codec)) {
			return make_error(ErrorCategory::State, ErrorCode::UnsupportedCodec, "unknown envelope codec: " + p_config.m_codec);
		}
		l_parsed.m_config = std::move(p_config);
		return l_parsed;
	}

	Result<std::shared_ptr<Transport>> open(const std::string& p_url, TransportConfig p_config) {
		auto l_parsed = parse_url(p_url, std::move(p_config));
		if (!l_parsed) {
			return l_parsed.error();
		}
		ParsedUrl& l_url = l_parsed.value();
		const auto l_codec = make_codec(l_url.m_config.m_codec);  // one codec per transport, so per namespace
		std::shared_ptr<Transport> l_transport;
		if (l_url.m_scheme == "loopback") {
			// The environments of one process that name the same loopback namespace share one transport:
			// that is what lets them see each other. A reused one is already open.
			static std::mutex s_mutex;
			static std::map<std::string, std::weak_ptr<LoopbackTransport>> s_loopbacks;
			const std::lock_guard<std::mutex> l_lock(s_mutex);
			if (auto l_shared = s_loopbacks[l_url.m_config.m_namespace].lock()) {
				if (l_shared->config().m_codec != l_url.m_config.m_codec) {
					return make_error(ErrorCategory::State, ErrorCode::InvalidConfiguration,
									  "loopback namespace " + l_url.m_config.m_namespace + " already uses the codec " + l_shared->config().m_codec);
				}
				return std::shared_ptr<Transport>(l_shared);
			}
			auto l_loopback = std::make_shared<LoopbackTransport>(std::move(l_url.m_config), LoopbackOptions{}, l_codec);
			if (const auto l_result = l_loopback->open(); !l_result) {
				return l_result.error();
			}
			s_loopbacks[l_loopback->config().m_namespace] = l_loopback;
			return std::shared_ptr<Transport>(l_loopback);
		} else if (is_mqtt(l_url.m_scheme)) {
#if CAM_TRANSPORT_MQTT
			MqttOptions l_options;
			for (const auto& [l_key, l_value]: l_url.m_options) {
				if (l_key == "mqtt") l_options.m_mqtt5 = l_value != "3.1.1";
			}
			l_transport = std::make_shared<MqttTransport>(std::move(l_url.m_config), l_options, l_codec);
#else
			return make_error(ErrorCategory::State, ErrorCode::NotSupported, "this build has no MQTT transport (CAM_TRANSPORT_MQTT)");
#endif
		} else {
#if CAM_TRANSPORT_ZENOH
			ZenohOptions l_options;
			if (!l_url.m_config.m_endpoint.empty()) {
				l_options.m_connect.push_back(l_url.m_config.m_endpoint);
			}
			for (const auto& [l_key, l_value]: l_url.m_options) {
				if (l_key == "mode") l_options.m_mode = l_value;
				else if (l_key == "scouting") l_options.m_multicast_scouting = true;
				else if (l_key == "listen") for (auto& l_endpoint: split_list(l_value)) l_options.m_listen.push_back(std::move(l_endpoint));
				else if (l_key == "connect") for (auto& l_endpoint: split_list(l_value)) l_options.m_connect.push_back(std::move(l_endpoint));
			}
			l_transport = std::make_shared<ZenohTransport>(std::move(l_url.m_config), std::move(l_options), l_codec);
#else
			return make_error(ErrorCategory::State, ErrorCode::NotSupported, "this build has no Zenoh transport (CAM_TRANSPORT_ZENOH)");
#endif
		}
		if (const auto l_result = l_transport->open(); !l_result) {
			return l_result.error();
		}
		return l_transport;
	}

	Result<void> connect(Environment& p_environment, const std::string& p_url, EndpointConfig p_config) {
		// Kept for the former callers: the environment owns its URL now.
		return p_environment.connect(p_url, std::move(p_config));
	}

} // namespace cam::transport
