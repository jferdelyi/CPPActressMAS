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
// oracle by iso.Vickrey.
// Port of the C# example E05 Auctions/Vickrey (ActressMAS v3.1): a sealed-bid second-price auction; every bidder bids
// its valuation once, the highest bidder wins and pays the second highest bid. Parallel, no turn limit; the valuations
// come from the seed of the C# oracle.

#include <algorithm>
#include <exception>
#include <functional>
#include <limits>
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
		struct Bid {
			std::string m_bidder;
			int m_bid_value;
		};

		std::vector<Bid> m_bids;
		int m_turns_to_wait = 0;

		void handle_bid(const std::string& p_sender, const int p_price) {
			m_bids.push_back({p_sender, p_price});
		}

		void handle_finish() {
			std::string l_highest_bidder;
			int l_highest_bid = std::numeric_limits<int>::min();
			std::vector<int> l_bid_values(m_bids.size());

			for (std::size_t i = 0; i < m_bids.size(); i++) {
				const int l_b = m_bids[i].m_bid_value;
				if (l_b > l_highest_bid && l_b >= Settings::s_reserve_price) {
					l_highest_bid = l_b;
					l_highest_bidder = m_bids[i].m_bidder;
				}
				l_bid_values[i] = l_b;
			}

			if (l_highest_bidder.empty()) {  // no bids above reserve price
				cam::dotnet::write_line("[auctioneer]: Auction finished. No winner.");
				broadcast("winner none");
			} else {
				std::sort(l_bid_values.begin(), l_bid_values.end());
				std::reverse(l_bid_values.begin(), l_bid_values.end());
				const int l_winning_price = l_bid_values.at(1);  // second price

				cam::dotnet::write_line("[auctioneer]: Auction finished. Sold to " + l_highest_bidder + " for price " + std::to_string(l_winning_price) + ".");
				broadcast("winner " + l_highest_bidder);
			}

			stop();
		}

	public:
		explicit AuctioneerAgent(const std::string& p_name) : Agent(p_name) {}

		void setup() override {
			broadcast("start");
			m_turns_to_wait = 2;
		}

		void action(const cam::MessagePointer& p_message) override {
			try {
				cam::dotnet::write_line("\t" + format(p_message));
				const auto [l_action, l_parameters] = p_message->parse_rest();

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

		void handle_start() const {
			send_by_name("auctioneer", "bid " + std::to_string(m_valuation));
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

				if (l_action == "start") {
					handle_start();
				} else if (l_action == "winner") {
					handle_winner(l_parameters);
				}
			} catch (const std::exception& p_exception) {
				cam::dotnet::write_line(p_exception.what());
			}
		}
};

int main() {
	cam::Environment l_environment({.m_turns = 0,
									.m_mode = cam::EnvironmentMasMode::Parallel,
									.m_seed = static_cast<unsigned int>(cam::dotnet::next_site_seed("EnvironmentMas.cs:61")),
									.m_random = cam::RandomEngine::DotNet});

	cam::DotnetRandom l_rand(cam::dotnet::next_site_seed("Program.cs:28"));

	for (int i = 1; i <= Settings::s_no_bidders; i++) {
		const int l_agent_valuation = Settings::s_min_price + l_rand.next(Settings::s_max_price - Settings::s_min_price);
		l_environment.add<BidderAgent>((i < 10 ? "bidder0" : "bidder") + std::to_string(i), l_agent_valuation);
	}

	l_environment.add<AuctioneerAgent>("auctioneer");

	l_environment.start();
	return 0;
}
