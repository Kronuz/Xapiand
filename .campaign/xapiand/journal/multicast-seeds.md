# multicast-seeds: journal

State: waiting on jobs 261120-092016-dd0882 261120-092045-08a731
Quest: .campaign/xapiand/quests/multicast-seeds.md (the lead's; never edit it)

Code: consensus worktree `consensus-wt/xapiand/multicast-seeds`, branch quest/xapiand/multicast-seeds,
consensus:32d9c9b (feature), consensus:db6d7f9 (failed() after joined), consensus:8a36958
(receiver only when seeding; burst 1..16). Not pushed (the lead pushes).

## Doing

Waiting on the suite at consensus:8a36958 (261120-092016-dd0882, queued) and the 300 s libFuzzer run
(261120-092045-08a731, running). Done and passing: seeds cases with multicast required
(261120-092018-d9b51c, exit 0) and all six mutants (each test failed at its mutant, ctest exit 8).
The suite at consensus:db6d7f9 passed (261120-091403-d07446), before the receiver/burst change.

## Next

1. When the jobs pass: paste `campaign brief`'s table, set State to "ready for review".
2. A failing mutant job (the test passed at the mutant) means a weak test: strengthen it, resubmit.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | build-test 261120-092016-dd0882; seeds cases (CONSENSUS_SEEDS_REQUIRE=1) 261120-092018-d9b51c: `processes`, `unavailable` |
| A2 | 261120-092018-d9b51c (`codec`, `drops`); mutants any-cluster 261120-092026-224cdc, unwanted-seeded 261120-092029-329d7f, trailing-accepted 261120-092033-db89f5 (--fails-without); libFuzzer 300 s 261120-092045-08a731; corpus replay `consensus_fuzz_seeds_replay` in ctest |
| A3 | 261120-092018-d9b51c (`forged`); mutant no-backoff 261120-092022-771814 (--fails-without) |
| A4 | 261120-092018-d9b51c (`rate`); mutants no-burst 261120-092039-8aa185, no-interval 261120-092042-ddf978 (--fails-without) |
| A5 | `docs/mechanisms/seeds.md` in consensus, consensus:8a36958 |

## Result

`include/consensus/transport/seeds.h` (`Seeds`: announce, listen, seed set, backoff) and
`seeds_codec.h` (DTSD0001, no I/O); `fuzz/seeds_fuzz.cc` + `fuzz/seeds_corpus/`;
`test/transport/seeds_test.cc` (cases codec, drops, processes, unavailable, forged, rate);
`test/transport/seeds_mutants.py`; how it works: `docs/mechanisms/seeds.md`; CHANGELOG 2026-11-20 Added.

Local run (macOS, loopback): three processes each report {9101, 9102, 9103}; forged: impostor
dialed 5 times in 2 s (backoff 100/200/400/800 ms) while forged every 10 ms, genuine node joined
once; rate: 14 datagrams in 1.5 s at interval 100 ms + jitter 50 ms after a 3-datagram burst.

Open for the lead: AGENTS.md asks each landed change be recorded in Detent's
`docs/LIBRARY_BOUNDARIES.md` (outside this file set). Xapiand's `--discovery-*` wiring is cluster's task.

## Rulings I made

- The codec is its own header (`seeds_codec.h`, std only) so the fuzz target and its replay link
  `consensus::consensus` without Asio.
- The seed address is the datagram's source address; the announcement names ports only, so a
  sender cannot aim listeners at a third host without spoofing its source.
- Version lives in the magic's digits (`DTSD0001`), like `DTRPnnnn`; another version is dropped
  and counted (`version`), never parsed.
- Sender identity is 16 random bytes per `Seeds`, used to drop a process's own announcements and to
  key seeds with the source address (three processes on one host share an address).
- Backoff state lives on the seed and survives re-announcement; an entry backing off is kept past
  `expire` until its backoff ends, so a returning forger resumes its backoff.
- `on_dial`/`failed`/`joined` instead of a dialer inside the library: the product dials with its own
  pins (Xapiand's join, Detent's voters/relays); the test's dial is TCP + `Connection::handshake`.
- Tests announce with TTL 0 on loopback, falling back to the default interface; a host where
  neither carries multicast skips with the reason (ctest exit 77), and CONSENSUS_SEEDS_REQUIRE=1
  makes that a failure (the evidence job sets it).
- `SO_REUSEPORT` beside `SO_REUSEADDR` so Xapiand and Detent processes on one host share the port.
- IPv4 interface by address, IPv6 by interface name; defaults 239.192.168.1:58880, TTL 3
  (Xapiand's pre-removal `config.h.in` and `discovery.cc`).
