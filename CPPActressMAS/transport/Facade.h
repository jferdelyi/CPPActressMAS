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

#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "../Environment.h"
#include "Transport.h"

/**
 * CPPActressMAS transport layer: the one-line entry point. The transports compiled in are chosen by the
 * scheme of a URL:
 *   loopback://[namespace]                               in-process transport, shared by the environments of the process that name it
 *   mqtt://host[:port][/namespace][?options]             MQTT broker (CAM_TRANSPORT_MQTT)
 *   mqtts://host[:port][/namespace][?options]            MQTT over TLS
 *   zenoh://[host:port][/namespace][?options]            Zenoh peer or router at host:port (CAM_TRANSPORT_ZENOH)
 * Common options: ns=, codec=protobuf|compact|json (protobuf by default). MQTT: mqtt=3.1.1, user=, password=, ca=,
 * cert=, key=, verify=0.
 * Zenoh: mode=peer|client, listen=tcp/host:port (repeatable, comma separated), scouting=multicast.
 * Without connect(), an environment stays offline (D4); a Zenoh URL needs a host, a listen option or
 * multicast scouting.
 */
namespace cam::transport {

	/**
	 * Schemes of the transports compiled in this build.
	 * @return The schemes ("loopback", "mqtt", "mqtts", "zenoh"...)
	 **/
	[[nodiscard]] std::vector<std::string> schemes();

	/**
	 * A transport URL taken apart: scheme, configuration, and the scheme-specific options of the query.
	 **/
	struct ParsedUrl {
		std::string m_scheme;
		std::string m_host;
		std::string m_port;
		TransportConfig m_config;
		std::vector<std::pair<std::string, std::string>> m_options;  ///< query pairs the scheme interprets
	};

	/**
	 * Parse a transport URL into a configuration.
	 * @param p_url The URL
	 * @param p_config Defaults to complete (namespace when the URL has none, timeouts...)
	 * @return The parsed URL, or an error (unknown scheme, unknown option, invalid namespace)
	 **/
	[[nodiscard]] Result<ParsedUrl> parse_url(const std::string& p_url, TransportConfig p_config = {});

	/**
	 * Open a transport from a URL (one connection, one namespace); share it between the environments of
	 * the process with Environment::connect.
	 * @param p_url The URL
	 * @param p_config Defaults completed by the URL
	 * @return The open transport, or an error (unknown scheme: NotSupported)
	 **/
	[[nodiscard]] Result<std::shared_ptr<Transport>> open(const std::string& p_url, TransportConfig p_config = {});

	/**
	 * Open a transport from a URL and connect an environment to it (one node per call).
	 * @param p_environment The environment
	 * @param p_url The URL
	 * @param p_config The endpoint configuration
	 * @return Success or an error
	 **/
	Result<void> connect(Environment& p_environment, const std::string& p_url, EndpointConfig p_config = {});

} // namespace cam::transport
