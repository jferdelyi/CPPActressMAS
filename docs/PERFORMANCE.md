# Performance

Measurements of CPPActressMAS against ActressMAS v3.1 (C#) and between its own transports and envelope codecs. Every
number below comes from a series run on one machine with alternated runs; a result is called faster only when the
quartiles of the two sides do not overlap. Medians are given with their first and third quartiles in parentheses.

## Method

- **Machine**: a Podman virtual machine on an Apple Silicon Mac (Linux arm64, 4 vCPU, 6 GiB), one container per run,
  Ubuntu 24.04, gcc 13.3, `-O3 -DNDEBUG` (the `bench` preset: static library, no transport, no `-march=native`). The
  numbers of a virtual machine are lower than those of the host; only the ratios compare from one series to another.
- **C#**: ActressMAS v3.1 (commit `eb0bb73` of [florinleon/ActressMas](https://github.com/florinleon/ActressMas)) under
  .NET 10.0.401 (runtime 10.0.12), in the same machine; C++ and C# runs alternate.
- **Timers**: inside the programs, around the same scope as the C# originals (`tests/bench/`, the measurement copies of
  the C# E30 benchmarks and of two models of Antelmi et al.).
- **Sizes**: the times of the C# at small sizes include its just-in-time compilation, which overstates the lead of
  the C++ there.

## C++ against C# (2026-10-27, C++ at commit `fbe0735`)

Benchmarks E30 of ActressMAS v3.1, 15 runs, the first trial of each run left out. Times in ms (lower is better),
throughputs in messages per ms (higher is better).

| Benchmark | C++ | C# | C++ faster |
|---|---|---|---|
| Skynet (ms) | 7.9 (7.0 / 8.6) | 38.0 (36.5 / 39.0) | ×4.8 |
| SkynetNumeric (ms) | 19.0 (17.3 / 19.7) | 92.0 (87.5 / 98.5) | ×4.8 |
| PingPong, parallel (msg/ms) | 12 688 (10 060 / 14 656) | 5 505 (4 717 / 6 034) | ×2.3 |
| PingPong, sequential (msg/ms) | 27 271 (26 765 / 28 569) | 11 796 (9 360 / 16 704) | ×2.3 |
| PingPong by pairs, parallel (msg/ms) | 4 573 (4 149 / 8 482) | 3 081 (2 743 / 3 791) | ×1.5 |
| PingPong by pairs, sequential (msg/ms) | 17 206 (16 808 / 18 007) | 5 351 (4 854 / 5 580) | ×3.2 |

In parallel mode the number of messages exchanged depends on the scheduling of the threads, in C# as in C++: the
parallel throughputs spread more than the sequential ones.

**Scaling**: Wolf, Sheep and Grass (WSG) and Flockers of Antelmi et al. (*Applied Sciences* 13(1), 13, 2023), 200
turns, 5 runs per size, times in ms:

| Agents | WSG C++ | WSG C# | Ratio | Flockers C++ | Flockers C# | Ratio |
|---|---|---|---|---|---|---|
| 1 000 | 28.8 | 117.4 | ×4.1 | 19.7 | 137.0 | ×7.0 |
| 4 000 | 115.1 | 459.6 | ×4.0 | 82.2 | 306.0 | ×3.7 |
| 16 000 | 791.6 | 2 326.2 | ×2.9 | 395.0 | 1 365.1 | ×3.5 |
| 64 000 | 8 143.4 | 16 013.5 | ×2.0 | 4 229.7 | 9 734.4 | ×2.3 |
| 128 000 | 22 595.4 (22 575 / 22 693) | 36 647.9 (36 411 / 36 684) | ×1.6 | 11 520.3 | 23 996.8 | ×2.1 |

The C++ is faster at every size, but its lead shrinks with the number of agents. Profiles of the 128 000 agents runs
show that most of the time goes to the code of the models themselves; the framework part was reduced since (below).

**Since that series**, the C++ was measured against itself only, in the same machine:
- the boundary of a turn sweeps the agents only when one of them died, in one pass: WSG at 128 000 agents went from
  21 210 ms (20 850 / 21 817) to 19 592 ms (19 371 / 20 117), −7.6 % (2026-10-30), with the same counts of wolves and
  sheep at every turn;
- the other benchmarks did not move beyond their noise.

## Transports (2026-10-04)

Two environments, each with its own transport instance, a ping agent and a pong agent answering by id; a round trip
costs at least two turn boundaries. 10 runs of 5 trials of 500 round trips, envelope codec `protobuf`, same machine,
built with the MQTT and Zenoh transports:

| Transport | Throughput (msg/ms) | Round trip (µs) |
|---|---|---|
| `loopback://`, serializing (no socket) | 216.5 (209.7 / 221.0) | 12 (7 / 14) |
| `zenoh://`, two peers on 127.0.0.1 | 15.96 (15.79 / 16.10) | 124 (121 / 130) |
| `mqtt://`, Mosquitto 2 on the same host, QoS 1 | 6.17 (5.46 / 6.61) | 367 (267 / 440) |

Zenoh is the fastest network transport and the least dispersed. MQTT went from 82 ms to about 0.3 ms per round trip
when the project moved from Paho to libmosquitto, which sets `TCP_NODELAY` on its socket.

## Envelope codecs (2026-10-04)

One message envelope (four identifiers, a content of 0, 64 or 1 024 bytes), 200 000 encodings then 200 000 decodings
per codec and size, decoding included its validation; 9 runs, ns per operation:

| Codec | Content (bytes) | Encode (ns) | Decode (ns) | Bytes on the wire |
|---|---|---|---|---|
| `protobuf` (default) | 0 | 129 | 248 | 205 |
| `protobuf` | 1 024 | 147 | 284 | 1 233 |
| `compact` | 0 | 185 | 258 | 240 |
| `compact` | 1 024 | 263 | 297 | 1 264 |
| `json` | 0 | 5 947 | 6 201 | 535 |
| `json` | 1 024 | 13 231 | 15 724 | 1 903 |

`protobuf`, written and read by the project without libprotobuf, is the default: it is the smallest and the fastest.
`json` is meant for reading the wire and for the simplest clients.

## Running the benchmarks

```bash
cmake --workflow --preset bench
build/bench/tests/bench/bench_Skynet
build/bench/tests/bench/bench_PingPong --sequential --trials 5
build/bench/tests/bench/bench_Wsg --agents 128000
build/bench/tests/bench/bench_Transport --transport loopback
```

The options of each program, and the `perf.vs_csharp` tests that compare the C++ with a reference of the C# measured on
the same machine, are described in [tests/bench/README.md](../tests/bench/README.md).
