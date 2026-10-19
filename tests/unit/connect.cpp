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

// Environment::connect(url): the URL belongs to the environment, the default factory knows the schemes
// compiled in, a custom factory adds its own, and the environments of a process that name the same
// loopback namespace share one transport. Loopback only: no network, no broker.

#include <doctest/doctest.h>

#include <Environment.h>
#include <transport/Facade.h>
#include <transport/loopback/LoopbackTransport.h>

#include <algorithm>

namespace {

	using namespace cam::transport;

	// Counts its turns and its end
	struct CountingEnvironment final : cam::Environment {
		using cam::Environment::Environment;
		int m_turns = 0;
		int m_finished = 0;
		void turn_finished(int) override { ++m_turns; }
		void simulation_finished() override { ++m_finished; }
	};

} // namespace

TEST_SUITE("connect") {

	TEST_CASE("an environment configured with a URL connects at its first start") {
		cam::Environment l_a({.m_turns = 1, .m_mode = cam::EnvironmentMasMode::Sequential, .m_url = "loopback://unit-connect-config",
							  .m_endpoint = {.m_name = "A"}});
		cam::Environment l_b({.m_turns = 1, .m_mode = cam::EnvironmentMasMode::Sequential, .m_url = "loopback://unit-connect-config",
							  .m_endpoint = {.m_name = "B"}});
		CHECK_FALSE(l_a.is_connected());
		CHECK(l_a.transport_url() == "loopback://unit-connect-config");
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

	TEST_CASE("a configured URL that does not connect throws before any turn, and start retries it") {
		CountingEnvironment l_env({.m_turns = 2, .m_mode = cam::EnvironmentMasMode::Sequential, .m_url = "carrier-pigeon://nowhere"});
		for (int l_attempt = 0; l_attempt < 2; l_attempt++) {
			try {
				l_env.start();
				FAIL("start() should have thrown");
			} catch (const TransportError& p_error) {
				CHECK(p_error.m_error.m_code == ErrorCode::NotSupported);
			}
		}
		CHECK(l_env.m_turns == 0);
		CHECK(l_env.m_finished == 0);
		CHECK_FALSE(l_env.is_connected());
	}

	TEST_CASE("after an explicit disconnect, start never reconnects") {
		cam::Environment l_env({.m_turns = 1, .m_mode = cam::EnvironmentMasMode::Sequential, .m_url = "loopback://unit-connect-once"});
		l_env.start();
		REQUIRE(l_env.is_connected());
		REQUIRE(l_env.disconnect());
		l_env.start();
		CHECK_FALSE(l_env.is_connected());
	}

	TEST_CASE("an environment without URL stays offline and says so") {
		cam::Environment l_env(1, cam::EnvironmentMasMode::Sequential);
		CHECK(l_env.transport_url().empty());
		CHECK_FALSE(l_env.is_connected());
		const auto l_result = l_env.connect();
		REQUIRE_FALSE(l_result);
		CHECK(l_result.error().m_code == ErrorCode::InvalidConfiguration);
		CHECK_FALSE(l_env.is_connected());
	}

	TEST_CASE("the URL is kept by the environment and used by connect()") {
		cam::Environment l_env(1, cam::EnvironmentMasMode::Sequential);
		l_env.set_transport_url("loopback://unit-connect-kept");
		CHECK(l_env.transport_url() == "loopback://unit-connect-kept");
		REQUIRE(l_env.connect());
		CHECK(l_env.is_connected());
		const auto l_again = l_env.connect();
		REQUIRE_FALSE(l_again);
		CHECK(l_again.error().m_code == ErrorCode::AlreadyConnected);
		REQUIRE(l_env.disconnect());
		CHECK_FALSE(l_env.is_connected());
		CHECK(l_env.transport_url() == "loopback://unit-connect-kept");  // still there for a later connect()
	}

	TEST_CASE("two environments naming the same loopback namespace see each other") {
		cam::Environment l_a(1, cam::EnvironmentMasMode::Sequential);
		cam::Environment l_b(1, cam::EnvironmentMasMode::Sequential);
		EndpointConfig l_name_a;
		l_name_a.m_name = "A";
		EndpointConfig l_name_b;
		l_name_b.m_name = "B";
		REQUIRE(l_a.connect("loopback://unit-connect-shared", l_name_a));
		REQUIRE(l_b.connect("loopback://unit-connect-shared", l_name_b));
		CHECK(l_a.transport_url() == "loopback://unit-connect-shared");
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
		cam::Environment l_a(1, cam::EnvironmentMasMode::Sequential);
		cam::Environment l_b(1, cam::EnvironmentMasMode::Sequential);
		REQUIRE(l_a.connect("loopback://unit-connect-one"));
		REQUIRE(l_b.connect("loopback://unit-connect-two"));
		l_a.start();
		l_b.start();
		CHECK(l_a.environments().empty());
		CHECK(l_b.environments().empty());
		REQUIRE(l_a.disconnect());
		REQUIRE(l_b.disconnect());
	}

	TEST_CASE("an unknown scheme or a transport not compiled in is refused before anything opens") {
		cam::Environment l_env(1, cam::EnvironmentMasMode::Sequential);
		const auto l_unknown = l_env.connect("carrier-pigeon://nowhere");
		REQUIRE_FALSE(l_unknown);
		CHECK(l_unknown.error().m_code == ErrorCode::NotSupported);
		CHECK_FALSE(l_env.is_connected());
		CHECK(l_env.transport_url() == "carrier-pigeon://nowhere");  // kept: the user may fix the build and retry
		const auto l_schemes = schemes();
		CHECK(std::find(l_schemes.begin(), l_schemes.end(), "loopback") != l_schemes.end());
		for (const char* l_scheme: {"mqtt", "zenoh"}) {
			if (std::find(l_schemes.begin(), l_schemes.end(), l_scheme) == l_schemes.end()) {
				const auto l_missing = l_env.connect(std::string(l_scheme) + "://127.0.0.1:1/unit");
				REQUIRE_FALSE(l_missing);
				CHECK(l_missing.error().m_code == ErrorCode::NotSupported);
			}
		}
	}

	TEST_CASE("a custom factory adds a scheme and falls back on the default for the others") {
		int l_calls = 0;
		auto l_shared = std::make_shared<LoopbackTransport>();
		REQUIRE(l_shared->open());
		cam::Environment l_env(1, cam::EnvironmentMasMode::Sequential);
		l_env.set_transport_factory([&](const std::string& p_url, TransportConfig p_config) -> Result<std::shared_ptr<Transport>> {
			l_calls++;
			if (p_url.rfind("fake://", 0) == 0) {
				return std::shared_ptr<Transport>(l_shared);
			}
			return open(p_url, std::move(p_config));
		});
		REQUIRE(l_env.connect("fake://anything"));
		CHECK(l_calls == 1);
		CHECK(l_env.is_connected());
		REQUIRE(l_env.disconnect());
		REQUIRE(l_env.connect("loopback://unit-connect-fallback"));  // the default handles it through the factory
		CHECK(l_calls == 2);
		REQUIRE(l_env.disconnect());
		l_env.set_transport_factory({});  // back to the default
		REQUIRE(l_env.connect("loopback://unit-connect-fallback"));
		CHECK(l_calls == 2);
		REQUIRE(l_env.disconnect());
	}

	TEST_CASE("the former facade connect is an alias of the environment's") {
		cam::Environment l_env(1, cam::EnvironmentMasMode::Sequential);
		REQUIRE(connect(l_env, "loopback://unit-connect-alias"));
		CHECK(l_env.transport_url() == "loopback://unit-connect-alias");
		REQUIRE(l_env.disconnect());
	}
}
