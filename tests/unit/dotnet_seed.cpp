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

// Seeds of the C# oracles for the examples (Examples/Common/DotNet.hpp, D96): the seeds that the seed shim of
// tools/csharp-ref gave, read in the "#camrng" lines of the oracle runs (CAM_SEED 0 and 42, 2026-10-19).

#include <doctest/doctest.h>

#include <cstdint>
#include <string>

#include <DotNet.hpp>

TEST_CASE("FNV-1a 64 gives the published vectors") {
	CHECK(cam::dotnet::fnv1a64("") == 0xcbf29ce484222325ULL);
	CHECK(cam::dotnet::fnv1a64("a") == 0xaf63dc4c8601ec8cULL);
	CHECK(cam::dotnet::fnv1a64("foobar") == 0x85944171f73967e8ULL);
}

TEST_CASE("site_seed gives the seeds of the oracle shim") {
	struct Vector {
		const char* m_site;
		std::uint64_t m_instance;
		std::uint64_t m_base;
		std::int32_t m_seed;
	};
	const Vector l_vectors[] = {
			{"EnvironmentMas.cs:61", 0, 0, 1644240549},
			{"EnvironmentMas.cs:61", 1, 0, 1921065420},
			{"Program.cs:119", 0, 0, 510404372},
			{"Program.cs:90", 0, 0, 932746367},
			{"Program.cs:36", 0, 0, 787071105},
			{"WorkerAgent.cs:9", 0, 0, 1092558013},
			{"EnvironmentMas.cs:61", 0, 42, 197252999},
			{"EnvironmentMas.cs:61", 1, 42, 1023968965},
			{"Program.cs:119", 0, 42, 755820040},
			{"Program.cs:90", 0, 42, 1342473750},
	};
	for (const auto& l_vector: l_vectors) {
		INFO(l_vector.m_site << " " << l_vector.m_instance << " " << l_vector.m_base);
		CHECK(cam::dotnet::site_seed(l_vector.m_site, l_vector.m_instance, l_vector.m_base) == l_vector.m_seed);
	}
}

TEST_CASE("next_site_seed counts the instances of each site") {
	const std::uint64_t l_base = cam::dotnet::cam_seed();
	CHECK(cam::dotnet::next_site_seed("Test.cs:1") == cam::dotnet::site_seed("Test.cs:1", 0, l_base));
	CHECK(cam::dotnet::next_site_seed("Test.cs:2") == cam::dotnet::site_seed("Test.cs:2", 0, l_base));
	CHECK(cam::dotnet::next_site_seed("Test.cs:1") == cam::dotnet::site_seed("Test.cs:1", 1, l_base));
	CHECK(cam::dotnet::next_site_seed("Test.cs:1") == cam::dotnet::site_seed("Test.cs:1", 2, l_base));
}
