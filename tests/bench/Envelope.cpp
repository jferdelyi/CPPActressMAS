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

// Micro-benchmark of the envelope codecs: encoding and decoding of one message
// envelope with a content of 0, 64 and 1 024 bytes, for every codec shipped. The decoded envelope is
// touched to keep the compiler honest. Usage: bench_Envelope [--count N] [--codec name]
// Output per codec and size: encode ns, decode ns, bytes on the wire.

#include <chrono>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include <transport/codec/EnvelopeCodec.h>

namespace {

	using namespace cam::transport;

	Envelope sample(const std::size_t p_content_size) {
		Envelope l_envelope;
		l_envelope.m_kind = EnvelopeKind::Message;
		l_envelope.m_sender_environment = "0f3c9d6e-1a2b-4c3d-8e9f-a0b1c2d3e4f5";
		l_envelope.m_epoch = 0x01020304;
		l_envelope.m_stream_kind = StreamKind::In;
		l_envelope.m_stream_target = "9e8d7c6b-5a4f-4e3d-9c2b-1a0f9e8d7c6b";
		l_envelope.m_sequence = 123456;
		l_envelope.m_conversation_id = "66666666-7777-4888-9999-aaaaaaaaaaaa";
		l_envelope.m_sender_agent = "11111111-2222-4333-8444-555555555555";
		l_envelope.m_target_kind = TargetKind::Id;
		l_envelope.m_target_id = "66666666-7777-4888-9999-aaaaaaaaaaaa";
		l_envelope.m_content_encoding = cam::MessageBinaryFormat::RAW;
		l_envelope.m_content.resize(p_content_size);
		for (std::size_t i = 0; i < p_content_size; i++) {
			l_envelope.m_content[i] = static_cast<std::uint8_t>(i * 31 + 7);
		}
		return l_envelope;
	}

	// Something the optimiser cannot drop.
	std::size_t touch(const Envelope& p_envelope) {
		return p_envelope.m_sender_environment.size() + p_envelope.m_target_id.size() + p_envelope.m_content.size() +
			   static_cast<std::size_t>(p_envelope.m_sequence & 1);
	}

	void measure(const EnvelopeCodec& p_codec, const std::size_t p_content_size, const long p_count) {
		const Envelope l_envelope = sample(p_content_size);
		const auto l_first = p_codec.encode(l_envelope);
		if (!l_first) {
			std::cerr << p_codec.name() << ": encode failed: " << l_first.error().m_message << std::endl;
			return;
		}
		std::size_t l_sink = 0;
		const auto l_encode_start = std::chrono::steady_clock::now();
		for (long i = 0; i < p_count; i++) {
			const auto l_bytes = p_codec.encode(l_envelope);
			l_sink += l_bytes.value().size();
		}
		const auto l_encode_end = std::chrono::steady_clock::now();
		for (long i = 0; i < p_count; i++) {
			const auto l_decoded = p_codec.decode(l_first.value(), 1u << 20);
			l_sink += touch(l_decoded.value());
		}
		const auto l_decode_end = std::chrono::steady_clock::now();
		const double l_encode_ns = std::chrono::duration<double, std::nano>(l_encode_end - l_encode_start).count() / static_cast<double>(p_count);
		const double l_decode_ns = std::chrono::duration<double, std::nano>(l_decode_end - l_encode_end).count() / static_cast<double>(p_count);
		std::cout << p_codec.name() << " content=" << p_content_size << " encode_ns=" << l_encode_ns << " decode_ns=" << l_decode_ns
				  << " bytes=" << l_first.value().size() << " sink=" << (l_sink & 1) << std::endl;
	}

} // namespace

int main(int argc, char** argv) {
	long l_count = 200000;
	std::string l_only;
	for (int i = 1; i < argc; i++) {
		if (std::strcmp(argv[i], "--count") == 0 && i + 1 < argc) {
			l_count = std::stol(argv[++i]);
		} else if (std::strcmp(argv[i], "--codec") == 0 && i + 1 < argc) {
			l_only = argv[++i];
		} else {
			std::cerr << "usage: bench_Envelope [--count N] [--codec protobuf|compact|json]" << std::endl;
			return 2;
		}
	}
	for (const auto& l_name: codec_names()) {
		if (!l_only.empty() && l_only != l_name) {
			continue;
		}
		const auto l_codec = make_codec(l_name);
		for (const std::size_t l_size: {std::size_t(0), std::size_t(64), std::size_t(1024)}) {
			measure(*l_codec, l_size, l_count);
		}
	}
	return 0;
}
