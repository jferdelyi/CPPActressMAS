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

#include "Agent.h"
#include "EnvironmentMas.h"

cam::Agent::Agent(const std::string& p_name) : 
	m_id(UUID::generate_uuid()),
	m_name(p_name),
	m_is_setup(false),
	m_observables(),
	m_environment(nullptr) {
}

const std::string& cam::Agent::get_id() const {
	return m_id;
}

const std::string& cam::Agent::get_name() const {
	return m_name;
}

bool cam::Agent::is_using_observables() const {
	return !m_observables.empty();
}

const std::unordered_map<std::string, std::string>& cam::Agent::get_observables() const {
	return m_observables;
}

bool cam::Agent::is_setup() const {
	return m_is_setup;
}

void cam::Agent::set_name(const std::string& p_name) {
	if (m_name == "") {
		return;
	}
	m_name = p_name;
}

void cam::Agent::set_environment(cam::EnvironmentMas* p_environment) {
	m_environment = p_environment;
}

void cam::Agent::internal_setup() {
	m_is_setup = true;
	setup();
}

void cam::Agent::internal_see() {
	see(m_environment->get_list_of_observable_agents(this));
}

void cam::Agent::internal_action() {
	if (m_messages.size_approx() > 0) {
		while (m_messages.size_approx() > 0) {
			MessagePointer l_message;
			if (m_messages.try_dequeue(l_message)) {
				action(l_message);
			}
		}
	} else {
		default_action();
	}
}

void cam::Agent::stop() {
	m_environment->remove(m_name);
}

void cam::Agent::post(const cam::MessagePointer& p_message) {
	m_messages.enqueue(p_message);
}

void cam::Agent::send(const std::string& p_receiver, const json& p_message) {
	m_environment->send(MessagePointer(new cam::Message(m_name, p_receiver, p_message)));
}

void cam::Agent::broadcast(const json& p_message) {
	m_environment->broadcast(m_name, p_message);
}

bool cam::Agent::perception_filter(const std::unordered_map<std::string, std::string>&) const {
	return false;
}

void cam::Agent::setup() {}

void cam::Agent::see(std::vector<ObservableAgentPointer>) {}

void cam::Agent::action(const cam::MessagePointer&) {}

void cam::Agent::default_action() {}

