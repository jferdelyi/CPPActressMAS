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

// Catalogue of the agent types of the process (Q27, D82): add<T>() names a type automatically, CAM_AGENT_TYPE(T) names
// it for a program that only receives it, conflicting names are refused, and an agent moves between two environments
// without any register_agent_type. Agents record into a Recorder and never assert; the tests check after start().

#include <doctest/doctest.h>

#include <stdexcept>
#include <string>
#include <typeindex>

#include <Environment.h>
#include <transport/loopback/LoopbackTransport.h>

#include "Recorder.hpp"

cam::test::Recorder g_catalogue_recorder;

// Types at global scope: they have a stable automatic name
struct CatalogueNamed final : cam::Agent {
	explicit CatalogueNamed(const std::string& p_name) : Agent(p_name) {}
};

struct CatalogueMacro final : cam::Agent {
	explicit CatalogueMacro(const std::string& p_name) : Agent(p_name) {}
};
CAM_AGENT_TYPE(CatalogueMacro);

struct CatalogueExplicit final : cam::Agent {
	explicit CatalogueExplicit(const std::string& p_name) : Agent(p_name) {}
};

struct CatalogueOther final : cam::Agent {
	explicit CatalogueOther(const std::string& p_name) : Agent(p_name) {}
};

// Moves once to a target environment, then records where it runs
struct CatalogueMover final : cam::Agent {
	explicit CatalogueMover(const std::string& p_name) : Agent(p_name) {}
	CatalogueMover(const std::string& p_name, std::string p_target) : Agent(p_name), m_target(std::move(p_target)) {}
	void default_action() override {
		if (!m_moved) {
			m_moved = true;
			g_catalogue_recorder.record(get_name() + ":move:" + (move(m_target) ? "ok" : "refused"));
			return;
		}
		g_catalogue_recorder.record(get_name() + ":running");
	}
	[[nodiscard]] json save_state() const override { return {{"target", m_target}, {"moved", m_moved}}; }
	void load_state(const json& p_state) override {
		m_target = p_state.at("target").get<std::string>();
		m_moved = p_state.at("moved").get<bool>();
	}
private:
	std::string m_target;
	bool m_moved = false;
};

namespace {

	struct Hidden final : cam::Agent {
		explicit Hidden(const std::string& p_name) : Agent(p_name) {}
	};

} // namespace

TEST_SUITE("agent_types") {

	TEST_CASE("add<T>() names a type of global scope automatically, never a type of an anonymous namespace") {
		cam::Environment l_env(1, cam::EnvironmentMasMode::Sequential);
		l_env.add<CatalogueNamed>("n");
		l_env.add<Hidden>("h");
		CHECK(cam::AgentTypes::name_of(std::type_index(typeid(CatalogueNamed))) == "CatalogueNamed");
		CHECK_FALSE(cam::AgentTypes::name_of(std::type_index(typeid(Hidden))).has_value());
		CHECK(l_env.is_agent_type_registered("CatalogueNamed"));
	}

	TEST_CASE("CAM_AGENT_TYPE names a type before any environment and builds it from its name") {
		CHECK(cam::AgentTypes::name_of(std::type_index(typeid(CatalogueMacro))) == "CatalogueMacro");
		const auto l_factory = cam::AgentTypes::factory_of("CatalogueMacro");
		REQUIRE(l_factory);
		CHECK(l_factory("built")->get_name() == "built");
	}

	TEST_CASE("an explicit name replaces the automatic one; conflicting names are refused") {
		cam::Environment l_env(1, cam::EnvironmentMasMode::Sequential);
		l_env.add<CatalogueExplicit>("e");
		CHECK(cam::AgentTypes::name_of(std::type_index(typeid(CatalogueExplicit))) == "CatalogueExplicit");
		l_env.register_agent_type<CatalogueExplicit>("Explicit");
		CHECK(cam::AgentTypes::name_of(std::type_index(typeid(CatalogueExplicit))) == "Explicit");
		CHECK_THROWS_AS(l_env.register_agent_type<CatalogueExplicit>("Other"), std::logic_error);  // two names for one type
		CHECK_THROWS_AS(l_env.register_agent_type<CatalogueOther>("Explicit"), std::logic_error);  // one name for two types
	}

	TEST_CASE("a list of accepted types restricts what may arrive") {
		cam::Environment l_env({.m_turns = 1, .m_mode = cam::EnvironmentMasMode::Sequential, .m_accepted_agent_types = std::vector<std::string>{}});
		CHECK_FALSE(l_env.is_agent_type_registered("CatalogueMacro"));
		l_env.register_agent_type<CatalogueMacro>("CatalogueMacro");  // accepted from now on
		CHECK(l_env.is_agent_type_registered("CatalogueMacro"));
	}

	TEST_CASE("an agent moves between two environments of the process without any register_agent_type") {
		g_catalogue_recorder.clear();
		auto l_transport = std::make_shared<cam::transport::LoopbackTransport>(cam::transport::TransportConfig{},
																			   cam::transport::LoopbackOptions{});
		REQUIRE(l_transport->open());
		cam::Environment l_a(1, cam::EnvironmentMasMode::Sequential);
		cam::Environment l_b(1, cam::EnvironmentMasMode::Sequential);
		REQUIRE(l_b.connect(l_transport, {.m_name = "B"}));
		REQUIRE(l_a.connect(l_transport, {.m_name = "A"}));
		const cam::AgentId l_id = l_a.add<CatalogueMover>("m", l_b.get_id());
		l_b.start();
		l_a.start();  // inserted, setup
		l_a.start();  // move: offer
		l_b.start();  // offer accepted: the catalogue builds the agent
		l_a.start();  // commit
		l_b.start();  // inserted, setup again
		l_b.start();  // runs at B
		CHECK(g_catalogue_recorder.contains("m:move:ok"));
		CHECK(g_catalogue_recorder.contains("m:running"));
		CHECK(l_a.agents_count() == 0);
		CHECK(l_b.get_agent_name(l_id) == "m");
		REQUIRE(l_a.disconnect());
		REQUIRE(l_b.disconnect());
	}
}
