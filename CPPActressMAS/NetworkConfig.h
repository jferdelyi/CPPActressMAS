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

#pragma once

#include <cstdint>
#include <string>

/**
 * CPPActressMAS
 **/
namespace cam {

	/**
	 * Protocol of an environment: None keeps it on the loopback of the process, joined without connect at its first
	 * start(); Loopback connects it to that loopback explicitly; the others reach the environments of other processes.
	 **/
	enum class Protocol {
		None,
		Loopback,
		Zenoh,
		Mqtt,
		Mqtts,
		ActressMas
	};

	/**
	 * Encoding of the envelopes on the wire, the same for every environment of a namespace.
	 **/
	enum class Codec {
		Protobuf,
		Compact,
		Json
	};

	/**
	 * Version of the MQTT protocol spoken to the broker.
	 **/
	enum class MqttVersion {
		V5,
		V3_1_1
	};

	/**
	 * Network of an environment, written with designated initializers:
	 * cam::Environment l_environment({.m_network = {.m_protocol = cam::Protocol::Zenoh, .m_namespace = "my-simulation"}});
	 * m_host:m_port is the meeting point of every protocol: the MQTT broker, the ActressMAS server, and with Zenoh the
	 * peer that listens there (the first environment that can listen there does, the others connect to it).
	 **/
	struct NetworkConfig {
		/**
		 * Protocol; None keeps the environment on the loopback of the process.
		 **/
		Protocol m_protocol = Protocol::None;

		/**
		 * Host of the meeting point (an address or a name).
		 **/
		std::string m_host = "127.0.0.1";

		/**
		 * Port of the meeting point; 0 for the port of the protocol (Zenoh 7447, MQTT 1883, MQTTS 8883, ActressMAS 5000).
		 **/
		std::uint16_t m_port = 0;

		/**
		 * Namespace of the environment, on the loopback of the process as on the network ([a-z0-9-]{1,32}): the
		 * environments of different namespaces never see each other.
		 **/
		std::string m_namespace = "default";

		/**
		 * Encoding of the envelopes on the wire.
		 **/
		Codec m_codec = Codec::Protobuf;

		/**
		 * MQTT: version of the protocol.
		 **/
		MqttVersion m_mqtt_version = MqttVersion::V5;

		/**
		 * MQTT: user name (empty: none).
		 **/
		std::string m_username{};

		/**
		 * MQTT: password (empty: none).
		 **/
		std::string m_password{};

		/**
		 * MQTTS: certificate of the authority that signed the broker (empty: the system ones).
		 **/
		std::string m_ca_file{};

		/**
		 * MQTTS: certificate of the client (empty: none).
		 **/
		std::string m_certificate_file{};

		/**
		 * MQTTS: private key of the client (empty: none).
		 **/
		std::string m_key_file{};

		/**
		 * MQTTS: check the certificate of the broker.
		 **/
		bool m_verify_peer = true;

		/**
		 * ActressMAS: name of the container of this environment on the server.
		 **/
		std::string m_container{};
	};

} // namespace cam
