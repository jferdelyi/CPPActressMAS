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

// The network of an environment (NetworkConfig): its defaults, the URL and the configuration each protocol builds,
// the role of Zenoh (listen on the meeting point, connect there otherwise), and the refusals. A factory of the
// environment records what it receives and gives a loopback transport: no network, no broker.

#include <doctest/doctest.h>

#include <stdexcept>
#include <string>
#include <vector>

#include <Environment.h>
#include <transport/loopback/LoopbackTransport.h>

namespace {

	using namespace cam::transport;
	using cam::Protocol;

	// Records every URL and configuration the environment gives it, refuses the URLs that contain m_refused, and gives
	// a loopback transport in the namespace of the configuration otherwise.
	struct Recording {
		std::vector<std::string> m_urls;
		std::vector<TransportConfig> m_configs;
		std::string m_refused;

		cam::Environment::TransportFactory factory() {
			return [this](const std::string& p_url, TransportConfig p_config) -> Result<std::shared_ptr<Transport>> {
				m_urls.push_back(p_url);
				m_configs.push_back(p_config);
				if (!m_refused.empty() && p_url.find(m_refused) != std::string::npos) {
					return make_error(ErrorCategory::Transport, ErrorCode::TransportFailure, "refused by the test");
				}
				auto l_transport = std::make_shared<LoopbackTransport>(std::move(p_config));
				if (const auto l_opened = l_transport->open(); !l_opened) {
					return l_opened.error();
				}
				return std::shared_ptr<Transport>(l_transport);
			};
		}
	};

} // namespace

TEST_SUITE("network_config") {

	TEST_CASE("a network is the loopback of the process by default, in the default namespace") {
		const cam::NetworkConfig l_network;
		CHECK(l_network.m_protocol == Protocol::None);
		CHECK(l_network.m_host == "127.0.0.1");
		CHECK(l_network.m_port == 0);
		CHECK(l_network.m_namespace == "default");
		CHECK(l_network.m_codec == cam::Codec::Protobuf);
		CHECK(l_network.m_mqtt_version == cam::MqttVersion::V5);
		CHECK(l_network.m_verify_peer);
		const cam::Environment l_env;
		CHECK(l_env.get_network().m_protocol == Protocol::None);
		CHECK(l_env.get_network().m_namespace == "default");
	}

	TEST_CASE("each protocol builds its URL on the meeting point, with the port of the protocol by default") {
		struct Case {
			Protocol m_protocol;
			std::uint16_t m_port;
			std::string m_container;
			std::string m_url;
		};
		for (const Case& l_case: {Case{Protocol::Loopback, 0, "", "loopback://"},
								  Case{Protocol::Zenoh, 0, "", "zenoh://?listen=tcp/127.0.0.1:7447"},
								  Case{Protocol::Mqtt, 0, "", "mqtt://127.0.0.1:1883"},
								  Case{Protocol::Mqtts, 0, "", "mqtts://127.0.0.1:8883"},
								  Case{Protocol::ActressMas, 0, "Container1", "actressmas://127.0.0.1:5000/Container1"},
								  Case{Protocol::Mqtt, 18831, "", "mqtt://127.0.0.1:18831"}}) {
			CAPTURE(l_case.m_url);
			Recording l_recording;
			cam::Environment l_env({.m_network = {.m_protocol = l_case.m_protocol, .m_port = l_case.m_port, .m_namespace = "unit-network",
												  .m_codec = cam::Codec::Json, .m_container = l_case.m_container}});
			l_env.set_transport_factory(l_recording.factory());
			REQUIRE(l_env.connect());
			REQUIRE(l_recording.m_urls.size() == 1);
			CHECK(l_recording.m_urls.front() == l_case.m_url);
			CHECK(l_recording.m_configs.front().m_namespace == "unit-network");  // never in the URL
			CHECK(l_recording.m_configs.front().m_codec == "json");
			CHECK(l_env.is_connected());
			REQUIRE(l_env.disconnect());
		}
	}

	TEST_CASE("MQTT: the version 3.1.1, the credentials and the TLS files reach the transport") {
		Recording l_recording;
		cam::Environment l_env({.m_network = {.m_protocol = Protocol::Mqtts, .m_host = "broker.local", .m_mqtt_version = cam::MqttVersion::V3_1_1,
											  .m_username = "user", .m_password = "secret", .m_ca_file = "ca.pem",
											  .m_certificate_file = "client.pem", .m_key_file = "client.key", .m_verify_peer = false}});
		l_env.set_transport_factory(l_recording.factory());
		REQUIRE(l_env.connect());
		REQUIRE(l_recording.m_urls.size() == 1);
		CHECK(l_recording.m_urls.front() == "mqtts://broker.local:8883?mqtt=3.1.1");
		const TransportConfig& l_config = l_recording.m_configs.front();
		CHECK(l_config.m_username == "user");
		CHECK(l_config.m_password == "secret");
		CHECK(l_config.m_ca_file == "ca.pem");
		CHECK(l_config.m_certificate_file == "client.pem");
		CHECK(l_config.m_key_file == "client.key");
		CHECK_FALSE(l_config.m_verify_peer);
		REQUIRE(l_env.disconnect());
	}

	TEST_CASE("with Zenoh, an environment that cannot listen on the meeting point connects to it") {
		Recording l_recording;
		l_recording.m_refused = "listen=";
		cam::Environment l_env({.m_network = {.m_protocol = Protocol::Zenoh, .m_port = 17470}});
		l_env.set_transport_factory(l_recording.factory());
		REQUIRE(l_env.connect());
		CHECK(l_recording.m_urls == std::vector<std::string>{"zenoh://?listen=tcp/127.0.0.1:17470", "zenoh://127.0.0.1:17470"});
		REQUIRE(l_env.disconnect());

		// Neither: the error of the connection, which names the listening tried first
		Recording l_refusing;
		l_refusing.m_refused = "zenoh://";
		cam::Environment l_alone({.m_network = {.m_protocol = Protocol::Zenoh, .m_port = 17470}});
		l_alone.set_transport_factory(l_refusing.factory());
		const auto l_result = l_alone.connect();
		REQUIRE_FALSE(l_result);
		CHECK(l_result.error().m_code == ErrorCode::TransportFailure);
		CHECK(l_result.error().m_message.find("listening on 127.0.0.1:17470") != std::string::npos);
		CHECK(l_refusing.m_urls.size() == 2);
	}

	TEST_CASE("nothing to connect to, or an ActressMAS network without its container, is refused") {
		cam::Environment l_env;
		const auto l_none = l_env.connect();
		REQUIRE_FALSE(l_none);
		CHECK(l_none.error().m_code == ErrorCode::InvalidConfiguration);
		Recording l_recording;
		cam::Environment l_actressmas({.m_network = {.m_protocol = Protocol::ActressMas}});
		l_actressmas.set_transport_factory(l_recording.factory());
		const auto l_no_container = l_actressmas.connect();
		REQUIRE_FALSE(l_no_container);
		CHECK(l_no_container.error().m_code == ErrorCode::InvalidConfiguration);
		CHECK(l_recording.m_urls.empty());
	}

	TEST_CASE("an invalid namespace is refused when the environment is built, and by set_network") {
		CHECK_THROWS_AS(cam::Environment({.m_network = {.m_namespace = "Not Valid"}}), std::invalid_argument);
		cam::Environment l_env;
		const auto l_result = l_env.set_network({.m_namespace = "UPPER"});
		REQUIRE_FALSE(l_result);
		CHECK(l_result.error().m_code == ErrorCode::InvalidConfiguration);
		CHECK(l_env.get_network().m_namespace == "default");
	}

	TEST_CASE("the network changes outside a connection only") {
		cam::Environment l_env({.m_network = {.m_protocol = Protocol::Loopback, .m_namespace = "unit-network-set"}});
		REQUIRE(l_env.connect());
		const auto l_connected = l_env.set_network({.m_protocol = Protocol::Loopback, .m_namespace = "unit-network-set-2"});
		REQUIRE_FALSE(l_connected);
		CHECK(l_connected.error().m_code == ErrorCode::AlreadyConnected);
		CHECK(l_env.get_network().m_namespace == "unit-network-set");
		REQUIRE(l_env.disconnect());
		REQUIRE(l_env.set_network({.m_protocol = Protocol::Loopback, .m_namespace = "unit-network-set-2"}));
		CHECK(l_env.get_network().m_namespace == "unit-network-set-2");
		REQUIRE(l_env.connect());
		CHECK(l_env.is_connected());
		REQUIRE(l_env.disconnect());
	}

	TEST_CASE("a transport of another namespace than the one of the environment is refused") {
		auto l_transport = std::make_shared<LoopbackTransport>(TransportConfig{.m_namespace = "unit-network-other"});
		REQUIRE(l_transport->open());
		cam::Environment l_default;
		const auto l_refused = l_default.connect(l_transport);
		REQUIRE_FALSE(l_refused);
		CHECK(l_refused.error().m_code == ErrorCode::InvalidConfiguration);
		CHECK_FALSE(l_default.is_connected());
		cam::Environment l_same({.m_network = {.m_namespace = "unit-network-other"}});
		REQUIRE(l_same.connect(l_transport));
		REQUIRE(l_same.disconnect());
	}
}
