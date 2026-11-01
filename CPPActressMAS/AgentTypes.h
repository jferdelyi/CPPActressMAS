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

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <typeindex>
#include <typeinfo>

#include "Agent.h"

/**
 * CPPActressMAS
 */
namespace cam {

	/**
	 * Wire name of an agent type fixed by CAM_AGENT_TYPE(T); no name otherwise.
	 **/
	template<typename T>
	struct AgentTypeName {};

	/**
	 * Catalogue of the agent types of the process, for migration: the wire name of each type, and a factory
	 * that builds an agent arriving under that name (T(const std::string& name), then load_state()). Filled by
	 * Environment::add<T>() (automatic name), CAM_AGENT_TYPE(T) and Environment::register_agent_type<T>(name).
	 * Thread-safe; one catalogue per process (a shared library holds it).
	 **/
	class AgentTypes final {
		public:
			/**
			 * Factory of an agent from its name.
			 **/
			using Factory = std::function<AgentPointer(const std::string&)>;

			/**
			 * Put a type in the catalogue under a name. An explicit name replaces an automatic one; two explicit names
			 * for one type, or one name for two types, are refused.
			 * @param p_type The type
			 * @param p_name The wire name
			 * @param p_factory The factory
			 * @param p_explicit True for a name given by CAM_AGENT_TYPE or register_agent_type
			 * @throw std::logic_error If the name or the type is already taken otherwise
			 **/
			static void add(std::type_index p_type, const std::string& p_name, Factory p_factory, bool p_explicit);

			/**
			 * Put a type in the catalogue under its automatic name: its qualified C++ name, without spaces; nothing for a
			 * type without a stable name (anonymous namespace, local class, template).
			 * @param p_type The type
			 * @param p_factory The factory
			 **/
			static void add_automatic(const std::type_info& p_type, Factory p_factory);

			/**
			 * Wire name of a type.
			 * @param p_type The type
			 * @return The name, or nothing if the type is not in the catalogue
			 **/
			[[nodiscard]] static std::optional<std::string> name_of(std::type_index p_type);

			/**
			 * Factory of a wire name.
			 * @param p_name The wire name
			 * @return The factory, empty if the name is not in the catalogue
			 **/
			[[nodiscard]] static Factory factory_of(const std::string& p_name);

			/**
			 * Put T in the catalogue, under the name of CAM_AGENT_TYPE(T) or under its automatic name, if it can be
			 * built from a name (a type that cannot is never moved).
			 * @return True, so that it can initialize a static
			 **/
			template<ConceptAgent T>
			static bool catalogue() {
				if constexpr (std::is_constructible_v<T, const std::string&>) {
					Factory l_factory = [](const std::string& p_name) -> AgentPointer { return std::make_shared<T>(p_name); };
					if constexpr (requires { AgentTypeName<T>::VALUE; }) {
						add(std::type_index(typeid(T)), AgentTypeName<T>::VALUE, std::move(l_factory), true);
					} else {
						add_automatic(typeid(T), std::move(l_factory));
					}
				}
				return true;
			}
	};

} // namespace cam

#define CAM_AGENT_TYPE_CONCAT_(p_left, p_right) p_left##p_right
#define CAM_AGENT_TYPE_CONCAT(p_left, p_right) CAM_AGENT_TYPE_CONCAT_(p_left, p_right)

/**
 * Give the agent type T a stable wire name (its name as written) and put it in the catalogue of the process at start-up,
 * so that a program that only receives T by migration can build it. At global scope, right after the class.
 * In a static library, a translation unit that only holds this line may be dropped by the linker.
 **/
#define CAM_AGENT_TYPE(T) \
	template<> struct cam::AgentTypeName<T> { static constexpr const char* VALUE = #T; }; \
	static const bool CAM_AGENT_TYPE_CONCAT(s_cam_agent_type_, __LINE__) = cam::AgentTypes::catalogue<T>()
