# multicast-seeds: journal

State: ready for review at consensus:8801b99; waiting on jobs 261120-111956-ba7a28 (seeds cases) 261120-111958-eba46c (suite); Linux CI after the lead's push
Quest: .campaign/xapiand/quests/multicast-seeds.md (the lead's; never edit it)

Code: consensus worktree `consensus-wt/xapiand/multicast-seeds`, branch quest/xapiand/multicast-seeds,
consensus:32d9c9b (feature), consensus:db6d7f9 (failed() after joined), consensus:8a36958
(receiver only when seeding; burst 1..16), consensus:8213720 (correctness review fixes F1, F2,
N1, N2, N5, N6). Not pushed (the lead pushes).

## Doing

Review fixes at consensus:8213720 (review `reviews/multicast-seeds-correctness.md`, lead's rulings):
- F1: a seed is its dial target (source address + asked-for services, sorted); sender only drops
  own announcements; `per_address` (16) caps one address's entries (`address_full`).
- F2: `required()` = `CI` or `CONSENSUS_SEEDS_REQUIRE`; vmactions/freebsd-vm SendEnv's `CI`.
- N1: `receive_failures`, reported once, next receive after the doubling backoff.
- N2: a failed setup retries after the doubling backoff (`setup_failures`); success clears
  `unavailable()`.
- N5: a throwing `on_dial` counts as `failed(id)`, the receive re-arms, the exception leaves run().
- N6: rate burst < 50 -> < 80 ms (+30), gap slack 60 -> 100 ms (+40), count >= 12 -> >= 10 (-2);
  forged impostor dials >= 3 -> >= 2 (-1). Lower (rate-limit) bounds unchanged.

Jobs at consensus:8213720:
- seeds cases, multicast required: 261120-100254-bede01 (exit 0, all 13 seeds tests Passed, none skipped); suite (verify): 261120-100256-fc6a26
- reviewer probe rerun: 261120-100407-204477 (prints PASS/FAIL, exits 0: read its output)
- mutants (--fails-without): no-backoff 261120-100323-1a8e61, any-cluster 261120-100326-b38b03,
  unwanted-seeded 261120-100328-759c29, trailing-accepted 261120-100331-e86063,
  no-burst 261120-100334-2f2af7, no-interval 261120-100336-6d4973,
  sender-keyed 261120-100339-a47bd4, no-address-cap 261120-100342-f417d9,
  no-setup-retry 261120-100345-e85866, receive-spins 261120-100348-7f49a7,
  throw-keeps-entry 261120-100351-c74582, ci-skips 261120-100354-550a87

SIGPIPE on Linux CI (run 44790460279; lead's msg), fixed at consensus:8801b99:
- Cause: the write is in the library, in `Connection` (connection.h), which the test's dial and
  impostor harness both use. OpenSSL drives the socket via `SSL_set_fd` and writes with plain
  `write()`. The transport sets `SO_NOSIGPIPE` where it exists; elsewhere (Linux) connection.h and
  host-connection.md make ignoring SIGPIPE the host's duty. Every other transport test does
  `signal(SIGPIPE, SIG_IGN)` (links, tls, peers, connection, rpc, groups, transport_idle); seeds_test
  did not. Seeds itself only sends UDP, which never raises SIGPIPE.
- Fix in the file set: seeds_test main ignores SIGPIPE `#ifndef SO_NOSIGPIPE`. Check: forged
  raises SIGPIPE first there, so the process dies on Linux unless the host ignores it (mutant
  `sigpipe-default`; only Linux, i.e. CI, can run it). macOS has SO_NOSIGPIPE, so local jobs
  cannot show it.
- Earlier jobs at consensus:8213720 were all green per the lead (suite, 12 mutants, probe fixed/rotate/starve PASS).

## Next

1. When the jobs pass: read the probe's output (rotate <= 5, starve PASS), paste `campaign brief`'s
   table, set State to "ready for review" at consensus:8213720.
2. A failing mutant job (the test passed at the mutant) means a weak test: strengthen it, resubmit.
3. CI at the landing commit must show the multicast cases Passed, not Skipped, on linux-asan-ubsan,
   linux-tsan and freebsd-x86_64 (F2).

## For the cluster wiring (no change here, lead's ruling)

- N3: destroying `Seeds` before `drained()` is a use-after-free (handlers hold `this`), asserted only
  in Debug and tests; same contract as `Links` (`links.h:71`). The wiring must stop() and drain.
- N4: under spoofing, a genuine address with other ports is now a new target with its own backoff;
  the genuine target's backoff is untouched. Inside the trusted-cluster model.

## Blocked on

Not blocking: whether `Connection` should stop leaving SIGPIPE to the host on Linux (a socket
BIO that sends with MSG_NOSIGNAL). That is outside this file set and changes the documented host
contract. Recommendation: keep the contract for now, and have the cluster wiring (Xapiand, Detent)
ignore SIGPIPE as these tests do.

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
- Sender identity is 16 random bytes per `Seeds`, used only to drop a process's own announcements.
- (F1) The seed key is (source address, asked-for services sorted by name, with ports): a set of
  services is what the caller dials. For one wanted service it is exactly address + service port.
  A changed port set is a new target (N4). Default `per_address` 16, validated 1..`seeds`.
- (F1) The flood in `forged_rotating` repeats one port: on loopback the genuine node shares the
  forger's address, so a port-rotating flood would take that address's cap by design; the cap is
  proven in `drops` (64 ports from one address, `per_address` 4: 4 held, rest `address_full`).
- (N1, N2) Setup retries and receive-error waits reuse `backoff`/`backoff_max` (1 s..60 s) and the
  one `backoff_after()` the dial backoff uses, reset on success; no new options.
- (N2) `on_unavailable` fires once per outage: a retry that succeeds clears `unavailable()`.
- (N5) A throwing `on_dial` is not swallowed: it counts as failed(id) and propagates out of run(),
  the receive re-armed first so the source keeps listening.
- (F2) `recovers` exhausts RLIMIT_NOFILE to fail setup, then restores it; `receive_errors` dup2s a
  pipe over the receiving descriptor inside on_dial so asio's speculative receive fails each time.
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
