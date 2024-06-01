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

#include <iostream>

#include <Environment.h>

class ContentInfo {
	public:
		int m_number;
		std::string m_text;

		explicit ContentInfo(const int l_number, std::string l_text) :
			m_number(l_number),
			m_text(std::move(l_text)) {}

		explicit ContentInfo(std::stringstream& p_stream) :
			m_number(0) {
			cereal::PortableBinaryInputArchive l_input_archive(p_stream);
			l_input_archive(*this);
		}

		[[nodiscard]] std::stringstream serialize_to_stream() const {
			std::stringstream l_stream;
			cereal::PortableBinaryOutputArchive l_output_archive(l_stream);
			l_output_archive(*this);
			return l_stream;
		}

		template<class Archive>
		void serialize(Archive& p_archive) {
			p_archive(m_number, m_text);
		}
};

class Agent1 final : public cam::Agent {
	public:
		explicit Agent1(const std::string& p_name) : Agent(p_name) { }

		void setup() override {
			for (int i = 0; i < 10; i++) {
				std::cout << "[" + get_name() + "]: setup " << std::to_string(i + 1) << std::endl;
				ContentInfo l_content(i + 1, "setup from a1*");
				send_by_name("a2", l_content.serialize_to_stream().str());
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
		}

		void action(const cam::MessagePointer& p_message) override {
			std::stringstream l_stream = std::stringstream(p_message->content().get<std::string>());
			const ContentInfo l_content(l_stream);
			std::cout << "[" + get_name() + "]: has received {" << l_content.m_text << ", " << std::to_string(l_content.m_number) << "}" << std::endl;
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
};

class Agent2 final : public cam::Agent {
	public:
		explicit Agent2(const std::string& p_name) : Agent(p_name) { }

		void setup() override {
			for (int i = 0; i < 3; i++) {
				std::cout << "[" + get_name() + "]: setup " << std::to_string(i + 1) << std::endl;
				ContentInfo l_content(i + 1, "setup from a2");
				send(get_first_agent_by_name("a1*").value(), l_content.serialize_to_stream().str());
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
		}

		void action(const cam::MessagePointer& p_message) override {
			std::stringstream l_stream = std::stringstream(p_message->content().get<std::string>());
			const ContentInfo l_content(l_stream);
			std::cout << "[" + get_name() + "]: has received {" << l_content.m_text << ", " << std::to_string(l_content.m_number) << "}" << std::endl;
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
};

int main() {
	cam::Environment l_environment(10);

	l_environment.add<Agent1>("a1*");
	l_environment.add<Agent2>("a2");
	l_environment.start();

	return 0;
}
