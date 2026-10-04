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
#include <vector>

#include "../Environment.h"
#include "Transport.h"

/**
 * CPPActressMAS transport layer: the one-line entry point. The transports compiled in are chosen by the
 * scheme of a URL:
 *   loopback://[namespace]                       in-process transport
 *   mqtt://host[:port][/namespace][?options]     MQTT broker (CAM_TRANSPORT_MQTT)
 *   mqtts://host[:port][/namespace][?options]    MQTT over TLS
 * Options: mqtt=3.1.1, user=, password=, ca=, cert=, key=, verify=0, codec=compact.
 * Without connect(), an environment stays offline (D4).
 */
namespace cam::transport {

	/**
	 * Schemes of the transports compiled in this build.
	 * @return The schemes ("loopback", "mqtt", "mqtts"...)
	 **/
	[[nodiscard]] std::vector<std::string> schemes();

	/**
	 * Parse a transport URL into a configuration.
	 * @param p_url The URL
	 * @param p_config Defaults to complete (namespace when the URL has none, timeouts...)
	 * @return The scheme and the configuration, or an error
	 **/
	struct ParsedUrl {
		std::string m_scheme;
		TransportConfig m_config;
	};
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
