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

// Harness program of the oracle tests (tests/iso): the C# original as it is, its bugs included, compared with its
// oracle by iso.EnglishWithBroadcast.
// Port of the C# example E05 Auctions/English with broadcast (ActressMAS v3.1): an open-cry English auction, every bid
// broadcast to everybody; the auctioneer prints the bids it receives, the bidders a dot per bid they hear, and the
// winner is announced once nobody bids for two turns. Sequential, fixed order, no turn limit; the valuations come from
// the seed of the C# oracle.

#include <exception>
#include <string>

#include <DotNet.hpp>
#include <Environment.h>
#include <util/DotnetRandom.h>

struct Settings {
	static constexpr int s_no_bidders = 5;
	static constexpr int s_reserve_price = 100;
	static constexpr int s_min_price = 50;
	static constexpr int s_max_price = 500;
	static constexpr int s_increment = 10;
};

class AuctioneerAgent final : public cam::Agent {
	private:
		std::string m_highest_bidder;
		int m_highest_bid = 0;
		int m_turns_to_wait = 0;

		void handle_bid(const std::string& p_sender, const int p_bid) {
			if (p_bid > m_highest_bid && p_bid >= Settings::s_reserve_price) {
				m_highest_bid = p_bid;
				m_highest_bidder = p_sender;
			}
		}

		void handle_finish() {
			cam::dotnet::write_line("[auctioneer]: Auction finished");

			if (!m_highest_bidder.empty()) {
				broadcast("winner " + m_highest_bidder);
			} else {
				broadcast("winner none");
			}

			stop();
		}

	public:
		explicit AuctioneerAgent(const std::string& p_name) : Agent(p_name) {}

		void setup() override {
			m_highest_bidder = "";
			m_highest_bid = 0;
			m_turns_to_wait = 2;

			broadcast("start");
		}

		void action(const cam::MessagePointer& p_message) override {
			try {
				cam::dotnet::write_line("\t" + format(p_message));
				const auto [l_action, l_parameters] = p_message->parse_rest();
				m_turns_to_wait = 2;

				if (l_action == "bid") {
					handle_bid(get_agent_name(p_message->get_sender()).value_or(""), cam::dotnet::to_int32(l_parameters));
				}
			} catch (const std::exception& p_exception) {
				cam::dotnet::write_line(p_exception.what());
			}
		}

		void default_action() override {
			if (--m_turns_to_wait <= 0) {
				handle_finish();
			}
		}
};

class BidderAgent final : public cam::Agent {
	private:
		int m_valuation;
		int m_current_bid = 0;
		bool m_participating = false;

		void handle_start() const {
			if (m_current_bid > 0) {
				broadcast("bid " + std::to_string(m_current_bid));
			}
		}

		void handle_bid(const int p_received_bid) {
			const int l_next = p_received_bid + Settings::s_increment;
			if (p_received_bid >= m_current_bid && l_next <= m_valuation) {
				m_current_bid = l_next;
				broadcast("bid " + std::to_string(l_next));
			}
		}

		void handle_winner(const std::string& p_winner) {
			if (p_winner == get_name()) {
				cam::dotnet::write_line("[" + get_name() + "]: I have won with " + std::to_string(m_current_bid));
			}

			stop();
		}

	public:
		BidderAgent(const std::string& p_name, const int p_valuation) : Agent(p_name), m_valuation(p_valuation) {
			if (m_valuation >= Settings::s_reserve_price) {
				m_current_bid = Settings::s_reserve_price;
				m_participating = true;
			} else {
				m_participating = false;
			}
		}

		void setup() override {
			cam::dotnet::write_line("[" + get_name() + "]: My valuation is " + std::to_string(m_valuation));
		}

		void action(const cam::MessagePointer& p_message) override {
			try {
				if (!m_participating) {
					stop();
				}

				int l_highest_bid = 0;

				const auto [l_action, l_parameters] = p_message->parse_rest();

				if (l_action == "start") {
					cam::dotnet::write_line("\r\n\t" + format(p_message));
					handle_start();
				} else if (l_action == "bid") {
					cam::dotnet::write(".");  // bids all-to-all
					const int l_bid = cam::dotnet::to_int32(l_parameters);
					if (l_bid > l_highest_bid) {
						l_highest_bid = l_bid;
					}
					if (l_highest_bid > 0) {
						handle_bid(l_highest_bid);
					}
				} else if (l_action == "winner") {
					cam::dotnet::write_line("\r\n\t" + format(p_message));
					handle_winner(l_parameters);
				}
			} catch (const std::exception& p_exception) {
				cam::dotnet::write_line(p_exception.what());
			}
		}

		void default_action() override {
			if (!m_participating) {
				stop();
			}
		}
};

int main() {
	// inefficient, uses broadcast to simulate public open-cry auction
	cam::Environment l_environment({.m_turns = 0,
									.m_mode = cam::EnvironmentMasMode::Sequential,
									.m_seed = static_cast<unsigned int>(cam::dotnet::next_site_seed("EnvironmentMas.cs:61")),
									.m_random = cam::RandomEngine::DotNet});

	l_environment.add<AuctioneerAgent>("auctioneer");

	cam::DotnetRandom l_rand(cam::dotnet::next_site_seed("Program.cs:33"));

	for (int i = 1; i <= Settings::s_no_bidders; i++) {
		const int l_agent_valuation = Settings::s_min_price + l_rand.next(Settings::s_max_price - Settings::s_min_price);
		l_environment.add<BidderAgent>((i < 10 ? "bidder0" : "bidder") + std::to_string(i), l_agent_valuation);
	}

	l_environment.start();
	return 0;
}
