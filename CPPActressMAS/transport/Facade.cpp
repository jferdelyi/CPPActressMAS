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

#include "LoopbackTransport.h"
#if CAM_TRANSPORT_MQTT
#include "mqtt/MqttTransport.h"
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

	} // namespace

	std::vector<std::string> schemes() {
		std::vector<std::string> l_schemes = {"loopback"};
#if CAM_TRANSPORT_MQTT
		l_schemes.emplace_back("mqtt");
		l_schemes.emplace_back("mqtts");
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

		if (l_parsed.m_scheme == "loopback") {
			if (!l_authority.empty()) {
				p_config.m_namespace = l_authority;
			}
		} else if (l_parsed.m_scheme == "mqtt" || l_parsed.m_scheme == "mqtts") {
			if (l_authority.empty()) {
				return invalid_url("mqtt URL without host: " + p_url);
			}
			std::string l_host = l_authority;
			std::string l_port = l_parsed.m_scheme == "mqtts" ? "8883" : "1883";
			if (const std::size_t l_colon = l_authority.rfind(':'); l_colon != std::string::npos && l_authority.find(']') == std::string::npos) {
				l_host = l_authority.substr(0, l_colon);
				l_port = l_authority.substr(l_colon + 1);
			}
			p_config.m_tls = l_parsed.m_scheme == "mqtts";
			p_config.m_endpoint = (p_config.m_tls ? "ssl://" : "tcp://") + l_host + ":" + l_port;
			if (!l_path.empty()) {
				p_config.m_namespace = l_path;
			}
		} else {
			return make_error(ErrorCategory::State, ErrorCode::NotSupported, "unknown transport scheme: " + l_parsed.m_scheme);
		}

		for (const auto& [l_key, l_value]: parse_query(l_query)) {
			if (l_key == "ns" || l_key == "namespace") p_config.m_namespace = l_value;
			else if (l_key == "user") p_config.m_username = l_value;
			else if (l_key == "password") p_config.m_password = l_value;
			else if (l_key == "ca") p_config.m_ca_file = l_value;
			else if (l_key == "cert") p_config.m_certificate_file = l_value;
			else if (l_key == "key") p_config.m_key_file = l_value;
			else if (l_key == "verify") p_config.m_verify_peer = l_value != "0" && l_value != "false";
			else if (l_key == "codec") p_config.m_codec = l_value;
			else if (l_key == "mqtt") { if (l_value != "5" && l_value != "3.1.1") return invalid_url("unknown MQTT version: " + l_value); }
			else return invalid_url("unknown transport option: " + l_key);
		}
		if (!is_valid_namespace(p_config.m_namespace)) {
			return invalid_url("invalid namespace: " + p_config.m_namespace);
		}
		if (p_config.m_codec != "compact") {
			return make_error(ErrorCategory::State, ErrorCode::UnsupportedCodec, "unknown envelope codec: " + p_config.m_codec);
		}
		// The MQTT version is read again by open(): keep the query for it.
		l_parsed.m_config = std::move(p_config);
		return l_parsed;
	}

	Result<std::shared_ptr<Transport>> open(const std::string& p_url, TransportConfig p_config) {
		auto l_parsed = parse_url(p_url, std::move(p_config));
		if (!l_parsed) {
			return l_parsed.error();
		}
		std::shared_ptr<Transport> l_transport;
		if (l_parsed.value().m_scheme == "loopback") {
			l_transport = std::make_shared<LoopbackTransport>(std::move(l_parsed.value().m_config));
		} else {
#if CAM_TRANSPORT_MQTT
			MqttOptions l_options;
			if (p_url.find("mqtt=3.1.1") != std::string::npos) {
				l_options.m_mqtt5 = false;
			}
			l_transport = std::make_shared<MqttTransport>(std::move(l_parsed.value().m_config), l_options);
#else
			return make_error(ErrorCategory::State, ErrorCode::NotSupported, "this build has no MQTT transport (CAM_TRANSPORT_MQTT)");
#endif
		}
		if (const auto l_result = l_transport->open(); !l_result) {
			return l_result.error();
		}
		return l_transport;
	}

	Result<void> connect(Environment& p_environment, const std::string& p_url, EndpointConfig p_config) {
		auto l_transport = open(p_url);
		if (!l_transport) {
			return l_transport.error();
		}
		return p_environment.connect(l_transport.value(), std::move(p_config));
	}

} // namespace cam::transport
