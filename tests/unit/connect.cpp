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

// Environment::connect(): the network belongs to the environment (NetworkConfig), the default factory knows the
// transports compiled in, a custom factory replaces the transport of a protocol, and the environments of a process that
// name the same loopback namespace share one transport. The facade still reads URLs (transport::open). Loopback only:
// no network, no broker.

#include <doctest/doctest.h>

#include <algorithm>

#include <Environment.h>
#include <transport/Facade.h>
#include <transport/loopback/LoopbackTransport.h>

namespace {

	using namespace cam::transport;
	using cam::Protocol;

	// Counts its turns and its end
	struct CountingEnvironment final : cam::Environment {
		using cam::Environment::Environment;
		int m_turns = 0;
		int m_finished = 0;
		void turn_finished(int) override { ++m_turns; }
		void simulation_finished() override { ++m_finished; }
	};

	// The loopback of a namespace, connected explicitly
	cam::NetworkConfig loopback(const std::string& p_namespace, const cam::Codec p_codec = cam::Codec::Protobuf) {
		return {.m_protocol = Protocol::Loopback, .m_namespace = p_namespace, .m_codec = p_codec};
	}

} // namespace

TEST_SUITE("connect") {

	TEST_CASE("an environment configured with a network connects at its first start") {
		cam::Environment l_a({.m_turns = 1, .m_mode = cam::EnvironmentMasMode::Sequential, .m_network = loopback("unit-connect-config"),
							  .m_endpoint = {.m_name = "A"}});
		cam::Environment l_b({.m_turns = 1, .m_mode = cam::EnvironmentMasMode::Sequential, .m_network = loopback("unit-connect-config"),
							  .m_endpoint = {.m_name = "B"}});
		CHECK_FALSE(l_a.is_connected());
		CHECK(l_a.get_network().m_namespace == "unit-connect-config");
		l_a.start();
		CHECK(l_a.is_connected());
		l_b.start();  // B learns A from its retained presence
		REQUIRE(l_b.environments().size() == 1);
		CHECK(l_b.environments()[0].m_name == "A");
		l_a.start();  // already connected: no second connection
		CHECK(l_a.is_connected());
		REQUIRE(l_a.disconnect());
		REQUIRE(l_b.disconnect());
	}

	TEST_CASE("a configured network that does not connect throws before any turn, and start retries it") {
		// An ActressMAS network without its container: refused before anything opens
		CountingEnvironment l_env({.m_turns = 2, .m_mode = cam::EnvironmentMasMode::Sequential,
								   .m_network = {.m_protocol = Protocol::ActressMas}});
		for (int l_attempt = 0; l_attempt < 2; l_attempt++) {
			try {
				l_env.start();
				FAIL("start() should have thrown");
			} catch (const TransportError& p_error) {
				CHECK(p_error.m_error.m_code == ErrorCode::InvalidConfiguration);
			}
		}
		CHECK(l_env.m_turns == 0);
		CHECK(l_env.m_finished == 0);
		CHECK_FALSE(l_env.is_connected());
	}

	TEST_CASE("after an explicit disconnect, start never reconnects") {
		cam::Environment l_env({.m_turns = 1, .m_mode = cam::EnvironmentMasMode::Sequential, .m_network = loopback("unit-connect-once")});
		l_env.start();
		REQUIRE(l_env.is_connected());
		REQUIRE(l_env.disconnect());
		l_env.start();
		CHECK_FALSE(l_env.is_connected());
	}

	TEST_CASE("an environment without network stays offline and says so") {
		cam::Environment l_env(1, cam::EnvironmentMasMode::Sequential);
		CHECK(l_env.get_network().m_protocol == Protocol::None);
		CHECK_FALSE(l_env.is_connected());
		const auto l_result = l_env.connect();
		REQUIRE_FALSE(l_result);
		CHECK(l_result.error().m_code == ErrorCode::InvalidConfiguration);
		CHECK_FALSE(l_env.is_connected());
	}

	TEST_CASE("the network is kept by the environment and used by connect()") {
		cam::Environment l_env(1, cam::EnvironmentMasMode::Sequential);
		REQUIRE(l_env.set_network(loopback("unit-connect-kept")));
		CHECK(l_env.get_network().m_namespace == "unit-connect-kept");
		REQUIRE(l_env.connect());
		CHECK(l_env.is_connected());
		const auto l_again = l_env.connect();
		REQUIRE_FALSE(l_again);
		CHECK(l_again.error().m_code == ErrorCode::AlreadyConnected);
		REQUIRE(l_env.disconnect());
		CHECK_FALSE(l_env.is_connected());
		CHECK(l_env.get_network().m_namespace == "unit-connect-kept");  // still there for a later connect()
	}

	TEST_CASE("two environments of the same loopback namespace see each other") {
		cam::Environment l_a({.m_turns = 1, .m_mode = cam::EnvironmentMasMode::Sequential, .m_network = loopback("unit-connect-shared"),
							  .m_endpoint = {.m_name = "A"}});
		cam::Environment l_b({.m_turns = 1, .m_mode = cam::EnvironmentMasMode::Sequential, .m_network = loopback("unit-connect-shared"),
							  .m_endpoint = {.m_name = "B"}});
		REQUIRE(l_a.connect());
		REQUIRE(l_b.connect());
		// B learnt A from its retained presence at connect; A learns B at its next turn boundary.
		REQUIRE(l_b.environments().size() == 1);
		CHECK(l_b.environments()[0].m_name == "A");
		l_a.start();
		REQUIRE(l_a.environments().size() == 1);
		CHECK(l_a.environments()[0].m_name == "B");
		REQUIRE(l_a.disconnect());
		REQUIRE(l_b.disconnect());
	}

	TEST_CASE("another loopback namespace is another transport") {
		cam::Environment l_a({.m_turns = 1, .m_mode = cam::EnvironmentMasMode::Sequential, .m_network = loopback("unit-connect-one")});
		cam::Environment l_b({.m_turns = 1, .m_mode = cam::EnvironmentMasMode::Sequential, .m_network = loopback("unit-connect-two")});
		REQUIRE(l_a.connect());
		REQUIRE(l_b.connect());
		l_a.start();
		l_b.start();
		CHECK(l_a.environments().empty());
		CHECK(l_b.environments().empty());
		REQUIRE(l_a.disconnect());
		REQUIRE(l_b.disconnect());
	}

	TEST_CASE("a transport not compiled in is refused before anything opens") {
		const auto l_schemes = schemes();
		CHECK(std::find(l_schemes.begin(), l_schemes.end(), "loopback") != l_schemes.end());
		for (const auto& [l_scheme, l_protocol]: {std::pair{"mqtt", Protocol::Mqtt}, std::pair{"zenoh", Protocol::Zenoh}}) {
			if (std::find(l_schemes.begin(), l_schemes.end(), l_scheme) == l_schemes.end()) {
				cam::Environment l_env({.m_turns = 1, .m_network = {.m_protocol = l_protocol, .m_port = 1}});
				const auto l_missing = l_env.connect();
				REQUIRE_FALSE(l_missing);
				CHECK(l_missing.error().m_code == ErrorCode::NotSupported);
				CHECK_FALSE(l_env.is_connected());
			}
		}
	}

	TEST_CASE("the facade refuses an unknown scheme or a malformed URL before anything opens") {
		const auto l_unknown = open("carrier-pigeon://nowhere");
		REQUIRE_FALSE(l_unknown);
		CHECK(l_unknown.error().m_code == ErrorCode::NotSupported);
		// Every scheme is parsed before its transport is looked for: the MQTT and Zenoh URLs below never reach a socket
		for (const char* l_url: {"no-scheme", "://no-scheme", "mqtt://", "mqtt://127.0.0.1:1/unit?mqtt=4", "zenoh://?mode=router",
								 "zenoh://?scouting=gossip", "loopback://unit-connect-bad?colour=red", "loopback://unit-connect-bad?ns=a+b",
								 "mqtt://127.0.0.1:1/unit?listen=tcp/127.0.0.1:1"}) {
			CAPTURE(l_url);
			const auto l_result = open(l_url);
			REQUIRE_FALSE(l_result);
			CHECK(l_result.error().m_code == ErrorCode::InvalidConfiguration);
		}
		const auto l_codec = open("loopback://unit-connect-bad?codec=morse");
		REQUIRE_FALSE(l_codec);
		CHECK(l_codec.error().m_code == ErrorCode::UnsupportedCodec);
	}

	TEST_CASE("a loopback namespace keeps the codec of the transport that opened it") {
		cam::Environment l_a({.m_turns = 1, .m_network = loopback("unit-connect-codec", cam::Codec::Json)});
		cam::Environment l_b({.m_turns = 1, .m_network = loopback("unit-connect-codec", cam::Codec::Compact)});
		REQUIRE(l_a.connect());
		const auto l_other = l_b.connect();
		REQUIRE_FALSE(l_other);
		CHECK(l_other.error().m_code == ErrorCode::InvalidConfiguration);
		REQUIRE(l_b.set_network(loopback("unit-connect-codec", cam::Codec::Json)));
		REQUIRE(l_b.connect());  // the same codec shares the transport
		REQUIRE(l_a.disconnect());
		REQUIRE(l_b.disconnect());
	}

	TEST_CASE("a custom factory replaces a transport and falls back on the default for the others") {
		int l_calls = 0;
		auto l_shared = std::make_shared<LoopbackTransport>(TransportConfig{.m_namespace = "unit-connect-fake"});
		REQUIRE(l_shared->open());
		cam::Environment l_env({.m_turns = 1, .m_network = loopback("unit-connect-fake")});
		l_env.set_transport_factory([&](const std::string& p_url, TransportConfig p_config) -> Result<std::shared_ptr<Transport>> {
			l_calls++;
			if (p_url == "loopback://" && p_config.m_namespace == "unit-connect-fake") {
				return std::shared_ptr<Transport>(l_shared);
			}
			return open(p_url, std::move(p_config));
		});
		REQUIRE(l_env.connect());
		CHECK(l_calls == 1);
		CHECK(l_env.is_connected());
		REQUIRE(l_env.disconnect());
		REQUIRE(l_env.set_network(loopback("unit-connect-fallback")));
		REQUIRE(l_env.connect());  // the default handles it through the factory
		CHECK(l_calls == 2);
		REQUIRE(l_env.disconnect());
		l_env.set_transport_factory({});  // back to the default
		REQUIRE(l_env.connect());
		CHECK(l_calls == 2);
		REQUIRE(l_env.disconnect());
	}
}
