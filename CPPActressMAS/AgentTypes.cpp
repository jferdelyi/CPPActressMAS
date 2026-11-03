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

#include "AgentTypes.h"

#include <algorithm>
#include <cstdlib>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <unordered_map>

#if defined(__GNUC__) || defined(__clang__)
#include <cxxabi.h>
#endif

namespace {

	struct Entry {
		std::string m_name;
		cam::AgentTypes::Factory m_factory;
		bool m_explicit = false;
	};

	// The catalogue, built at its first use (no static initialization order to depend on)
	struct Catalogue {
		std::shared_mutex m_mutex;
		std::unordered_map<std::type_index, Entry> m_by_type;
		std::unordered_map<std::string, std::type_index> m_by_name;
	};

	Catalogue& the_catalogue() {
		static Catalogue s_catalogue;
		return s_catalogue;
	}

	// Qualified C++ name of a type without spaces, empty when it is not stable across programs
	std::string readable_name(const std::type_info& p_type) {
		std::string l_name;
#if defined(__GNUC__) || defined(__clang__)
		int l_status = 0;
		char* const l_demangled = abi::__cxa_demangle(p_type.name(), nullptr, nullptr, &l_status);
		if (l_status == 0 && l_demangled) {
			l_name = l_demangled;
		}
		std::free(l_demangled);
#else
		l_name = p_type.name();  // MSVC: "class ns::Name" or "struct ns::Name"
		for (const char* l_prefix: {"class ", "struct "}) {
			if (l_name.rfind(l_prefix, 0) == 0) {
				l_name.erase(0, std::char_traits<char>::length(l_prefix));
			}
		}
#endif
		// Anonymous namespace, local class, lambda or template: no name another program would give the same type
		if (l_name.empty() || l_name.find("anonymous") != std::string::npos || l_name.find_first_of("()<>`'") != std::string::npos) {
			return {};
		}
		l_name.erase(std::remove(l_name.begin(), l_name.end(), ' '), l_name.end());
		return l_name;
	}

} // namespace

void cam::AgentTypes::add(const std::type_index p_type, const std::string& p_name, Factory p_factory, const bool p_explicit) {
	Catalogue& l_catalogue = the_catalogue();
	const std::unique_lock<std::shared_mutex> l_lock(l_catalogue.m_mutex);
	if (const auto l_named = l_catalogue.m_by_name.find(p_name); l_named != l_catalogue.m_by_name.end() && l_named->second != p_type) {
		throw std::logic_error("agent type name " + p_name + " already given to another type");
	}
	if (const auto l_known = l_catalogue.m_by_type.find(p_type); l_known != l_catalogue.m_by_type.end()) {
		Entry& l_entry = l_known->second;
		if (l_entry.m_name == p_name) {
			l_entry.m_explicit = l_entry.m_explicit || p_explicit;
			return;
		}
		if (!p_explicit) {
			return;  // an automatic name never replaces a name already given
		}
		if (l_entry.m_explicit) {
			throw std::logic_error("agent type already named " + l_entry.m_name + ", not " + p_name);
		}
		l_catalogue.m_by_name.erase(l_entry.m_name);
		l_entry = Entry{p_name, std::move(p_factory), true};
		l_catalogue.m_by_name.emplace(p_name, p_type);
		return;
	}
	l_catalogue.m_by_type.emplace(p_type, Entry{p_name, std::move(p_factory), p_explicit});
	l_catalogue.m_by_name.emplace(p_name, p_type);
}

void cam::AgentTypes::add_automatic(const std::type_info& p_type, Factory p_factory) {
	if (const std::string l_name = readable_name(p_type); !l_name.empty()) {
		add(std::type_index(p_type), l_name, std::move(p_factory), false);
	}
}

std::optional<std::string> cam::AgentTypes::name_of(const std::type_index p_type) {
	Catalogue& l_catalogue = the_catalogue();
	const std::shared_lock<std::shared_mutex> l_lock(l_catalogue.m_mutex);
	const auto l_known = l_catalogue.m_by_type.find(p_type);
	return l_known == l_catalogue.m_by_type.end() ? std::nullopt : std::optional<std::string>(l_known->second.m_name);
}

cam::AgentTypes::Factory cam::AgentTypes::factory_of(const std::string& p_name) {
	Catalogue& l_catalogue = the_catalogue();
	const std::shared_lock<std::shared_mutex> l_lock(l_catalogue.m_mutex);
	const auto l_named = l_catalogue.m_by_name.find(p_name);
	return l_named == l_catalogue.m_by_name.end() ? Factory() : l_catalogue.m_by_type.at(l_named->second).m_factory;
}
