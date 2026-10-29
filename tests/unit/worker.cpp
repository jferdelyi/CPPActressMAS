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

// Worker of the network transports: one thread running jobs in order. A job posted before the start runs once the
// worker starts (the Zenoh transport posts the presence requests of the liveliness history while it opens), and a
// stop drops the jobs still pending. Jobs record into a Recorder; the tests check from their own thread.

#include <doctest/doctest.h>

#include <chrono>
#include <string>
#include <vector>

#include <transport/network/Worker.h>

#include "Recorder.hpp"
#include "wait_until.hpp"

using namespace std::chrono_literals;

TEST_SUITE("unit") {

	TEST_CASE("a worker runs the jobs posted before its start, in order, then the later ones") {
		cam::test::Recorder l_recorder;
		cam::transport::Worker l_worker;
		l_worker.post([&l_recorder] { l_recorder.record("first"); });
		l_worker.post([&l_recorder] { l_recorder.record("second"); });
		l_worker.start();
		l_worker.post([&l_recorder] { l_recorder.record("third"); });
		CHECK(cam::test::wait_until([&l_recorder] { return l_recorder.count() == 3; }, 5s));
		l_worker.stop();
		CHECK(l_recorder.entries() == std::vector<std::string>{"first", "second", "third"});
	}

	TEST_CASE("a stopped worker drops its pending jobs and runs the ones posted after its new start") {
		// Jobs run in order: a pending job kept by the stop would run before the marker
		cam::test::Recorder l_recorder;
		cam::transport::Worker l_worker;
		l_worker.post([&l_recorder] { l_recorder.record("dropped"); });
		l_worker.stop();
		l_worker.start();
		l_worker.post([&l_recorder] { l_recorder.record("marker"); });
		CHECK(cam::test::wait_until([&l_recorder] { return l_recorder.contains("marker"); }, 5s));
		l_worker.stop();
		CHECK(l_recorder.entries() == std::vector<std::string>{"marker"});
	}

}
