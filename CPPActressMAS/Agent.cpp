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

cam::Agent::Agent(std::string p_name) :
	m_id(UUID::generate_uuid()),
	m_name(std::move(p_name)),
	m_is_setup(false),
	m_observables(new Observables()),
	m_environment(nullptr) {
}

const std::string& cam::Agent::get_id() const {
	return m_id;
}

const std::string& cam::Agent::get_name() const {
	return m_name;
}

bool cam::Agent::is_using_observables() const {
	return !m_observables->empty();
}

cam::ObservablesPointer cam::Agent::get_observables() const {
	return m_observables;
}

bool cam::Agent::is_setup() const {
	return m_is_setup;
}

void cam::Agent::set_environment(EnvironmentMas* p_environment) {
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
			if (MessagePointer l_message; m_messages.try_dequeue(l_message)) {
				action(l_message);
			}
		}
	} else {
		default_action();
	}
}

void cam::Agent::stop() const {
	m_environment->remove(m_id);
}

void cam::Agent::post(const MessagePointer& p_message) {
	m_messages.enqueue(p_message);
}

void cam::Agent::send(const std::string& p_receiver_id, const json& p_message) {
	m_environment->send(std::make_shared<Message>(m_id, p_receiver_id, p_message));
}

void cam::Agent::broadcast(const json& p_message) const {
	m_environment->broadcast(m_id, p_message);
}

bool cam::Agent::perception_filter(const ObservablesPointer&) const {
	return false;
}

void cam::Agent::setup() {}

void cam::Agent::see(const std::vector<const ObservablesPointer>&) {}

void cam::Agent::action(const MessagePointer&) {}

void cam::Agent::default_action() {}

