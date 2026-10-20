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
// oracle by iso.EnglishWithoutBroadcast.
// Port of the C# example E05 Auctions/English without broadcast (ActressMAS v3.1): an English auction where each bidder
// only talks to the auctioneer; the auctioneer raises the price while two bidders or more still bid. Parallel, no turn
// limit; the valuations come from the seed of the C# oracle. Among the bidders of a round, the first whose bid arrives
// is kept, as in the C#: with equal bids, the winner depends on the interleaving.

#include <exception>
#include <string>
#include <vector>

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
		std::vector<std::string> m_bidders;
		std::string m_highest_bidder;
		int m_current_price = 0;
		int m_turns_to_wait = 0;

		void handle_bid(const std::string& p_sender) {
			m_bidders.push_back(p_sender);
		}

		void handle_no_bids() {
			if (m_bidders.empty()) {  // no more bids
				m_current_price -= Settings::s_increment;
				if (m_current_price < Settings::s_reserve_price) {
					cam::dotnet::write_line("[auctioneer]: Auction finished. No winner.");
					broadcast("winner none");
				} else {
					cam::dotnet::write_line("[auctioneer]: Auction finished. Sold to " + m_highest_bidder + " for price " + std::to_string(m_current_price) + ".");
					broadcast("winner " + m_highest_bidder);
				}
				stop();
			} else if (m_bidders.size() == 1) {
				m_highest_bidder = m_bidders[0];
				cam::dotnet::write_line("[auctioneer]: Auction finished. Sold to " + m_highest_bidder + " for price " + std::to_string(m_current_price));
				broadcast("winner " + m_highest_bidder);
				stop();
			} else {
				m_highest_bidder = m_bidders[0];  // first or random from the previous round, breaking ties
				m_current_price += Settings::s_increment;

				for (const auto& l_bidder: m_bidders) {
					send_by_name(l_bidder, "price " + std::to_string(m_current_price));
				}

				m_bidders.clear();
				m_turns_to_wait = 2;
			}
		}

	public:
		explicit AuctioneerAgent(const std::string& p_name) : Agent(p_name) {}

		void setup() override {
			m_bidders.clear();

			m_current_price = Settings::s_reserve_price;

			broadcast("price " + std::to_string(m_current_price));
			m_turns_to_wait = 2;
		}

		void action(const cam::MessagePointer& p_message) override {
			try {
				cam::dotnet::write_line("\t" + format(p_message));

				const auto [l_action, l_parameters] = p_message->parse_rest();

				if (l_action == "bid") {
					handle_bid(get_agent_name(p_message->get_sender()).value_or(""));
				}
			} catch (const std::exception& p_exception) {
				cam::dotnet::write_line(p_exception.what());
			}
		}

		void default_action() override {
			if (--m_turns_to_wait <= 0) {
				handle_no_bids();
			}
		}
};

class BidderAgent final : public cam::Agent {
	private:
		int m_valuation;

		void handle_price(const int p_current_price) {
			if (p_current_price <= m_valuation) {
				send_by_name("auctioneer", "bid");
			} else {
				stop();
			}
		}

		void handle_winner(const std::string& p_winner) {
			if (p_winner == get_name()) {
				cam::dotnet::write_line("[" + get_name() + "]: I have won.");
			}

			stop();
		}

	public:
		BidderAgent(const std::string& p_name, const int p_valuation) : Agent(p_name), m_valuation(p_valuation) {}

		void setup() override {
			cam::dotnet::write_line("[" + get_name() + "]: My valuation is " + std::to_string(m_valuation));
		}

		void action(const cam::MessagePointer& p_message) override {
			try {
				cam::dotnet::write_line("\t" + format(p_message));
				const auto [l_action, l_parameters] = p_message->parse_rest();

				if (l_action == "price") {
					handle_price(cam::dotnet::to_int32(l_parameters));
				} else if (l_action == "winner") {
					handle_winner(l_parameters);
				}
			} catch (const std::exception& p_exception) {
				cam::dotnet::write_line(p_exception.what());
			}
		}
};

int main() {
	// more efficient, each bidder only talks to the auctioneer
	cam::Environment l_environment({.m_turns = 0,
									.m_mode = cam::EnvironmentMasMode::Parallel,
									.m_seed = static_cast<unsigned int>(cam::dotnet::next_site_seed("EnvironmentMas.cs:61")),
									.m_random = cam::RandomEngine::DotNet});

	cam::DotnetRandom l_rand(cam::dotnet::next_site_seed("Program.cs:30"));

	for (int i = 1; i <= Settings::s_no_bidders; i++) {
		const int l_agent_valuation = Settings::s_min_price + l_rand.next(Settings::s_max_price - Settings::s_min_price);
		l_environment.add<BidderAgent>((i < 10 ? "bidder0" : "bidder") + std::to_string(i), l_agent_valuation);
	}

	l_environment.add<AuctioneerAgent>("auctioneer");

	l_environment.start();
	return 0;
}
