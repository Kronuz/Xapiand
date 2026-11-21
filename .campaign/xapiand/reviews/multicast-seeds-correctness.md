# multicast-seeds review: correctness

Commit: consensus:8a369580128b7095bbb41cb08fc8e6ca06eba4ef
Verdict: CHANGES
Date: 2026-11-20

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- 261120-092018-d9b51c (exit 0, `CONSENSUS_SEEDS_REQUIRE=1`, macOS, Release with `-UNDEBUG` on the
  test target): all six seeds cases and `consensus_fuzz_seeds_replay` passed, none skipped.
- 261120-092045-08a731 (exit 0): libFuzzer with ASan and UBSan, 300 s, 22,493,429 runs, 372 new
  units, peak RSS 479 MB, no crash. With my read of `seeds_codec.h:82-113` (every index is behind
  a size check; at most 8 names of at most 255 bytes inside a 512-byte input) this settles
  decoder safety: no out-of-bounds read, no unbounded allocation, no crash.
- The six mutant jobs (--fails-without) for no-backoff, any-cluster, unwanted-seeded,
  trailing-accepted, no-burst and no-interval: each case fails without what it tests.
- Not relied on: build-test 261120-092016-dd0882 (running when I wrote this) and CI run
  44784318123 (every platform job queued).

## What I checked myself

Probe job 261120-093739-4d849e at consensus:8a36958, scripts in
`Xapiand/.scratch/xapiand/review-multicast-seeds-correctness/` (`run.sh` builds `probe.cc` against
the checkout's `consensus::transport`; rerun at a fix with the same command). The dial always fails
at once, as a failed TLS hello would; announcements on loopback, TTL 0, backoff 100 ms:

- `fixed`, one forged sender every 10 ms for 2 s: 5 dials of the impostor (the `forged` case's bound).
- `rotate`, the same forger with a new 16-byte sender field per datagram: 191 dials of the same
  impostor address and port in 2 s, one per datagram.
- `starve`, 256 forged senders from one address refreshed every 500 ms, then a genuine node
  announcing for 3 s: the genuine node is never seeded (52 of its announcements counted `full`,
  0 dials).

Read: `seeds.h` (all of it) for the four questions, `seeds_test.cc`, `seeds.md`, the CMake and
CHANGELOG diff, `seeds_mutants.py`, and `links.h:71` for the library's drain contract.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | defect | Backoff and the seed set are keyed by (source address, sender), and the sender is 16 bytes the forger chooses. Rotating it defeats A3: the same failing address and port is dialed once per datagram, not backed off. It also lets one source address fill the 256-entry set, so a node that starts later is never seeded, contradicting `seeds.md` ("a genuine node is found through its own announcements whatever a forger sends"). The sender field is fine for dropping a node's own announcements; it is not a fit key for dial state. | probe 261120-093739-4d849e: `rotate` 191 dials in 2 s against 5 for `fixed`; `starve` seeded=0, full=52; `seeds.h:339-345`, `seeds.h:350` | Key dial state and backoff by the dial target (source address and service port), and cap the entries one source address may hold (several processes per host need a few, not 256), so a forger without spoofing can neither skip the backoff nor fill the set. Add to `seeds_test` a `forged` variant whose forger rotates the sender every datagram: the impostor gets at most five dials in 2 s, and a genuine node that starts after a 256-sender flood from one address still joins. If the owner accepts the set-fill under the trusted-cluster model instead, `seeds.md` drops the "whatever a forger sends" claim and the rotation bound still lands. |
| F2 | yes | gap | Linux and FreeBSD are unproven, and CI cannot fail these cases. The skip decision comes from `Seeds` itself (`choose_interface` and `require_multicast`, `seeds_test.cc:90-114`, `:283-286`), so a platform bug that stops `Seeds` hearing or joining makes every multicast case "Skipped", which ctest and CI count as green. Only the macOS evidence job sets `CONSENSUS_SEEDS_REQUIRE`; the shared `cmake.yml` does not. Exit 77 is the right default for a developer sandbox, not for CI. | `seeds_test.cc:86-89`; CMakeLists `SKIP_RETURN_CODE 77`; CI run 44784318123 not started | Treat CI as required: the test also requires multicast when `CI` is set (GitHub Actions sets it; check that the FreeBSD VM job passes it through), or the CMake sets `CONSENSUS_SEEDS_REQUIRE` in the test ENVIRONMENT under an option that is ON in CI. The CI run at the landing commit must show the five multicast cases Passed, not Skipped, on linux-asan-ubsan, linux-tsan and freebsd-x86_64. |
| N1 | no | nit | A receive error other than an abort is neither counted nor reported, and the handler re-arms at once, so a persistent error spins the loop silently. | `seeds.h:310-314` | Count it in `SeedStats` and report it once, like `send_failures`. |
| N2 | no | gap | A setup failure is final: a node started before its network is up (a laptop on Wi-Fi, DHCP) never seeds until restart, while a send failure keeps retrying. The doc states this, so it is a choice, not a bug. | `seeds.h:161-162`, `seeds.md` "Unavailable multicast" | Owner's call: keep it, or retry `start()` on the announce interval after a setup failure. |
| N3 | no | nit | Destroying `Seeds` before `drained()` is a use-after-free (handlers capture `this` and `Pending`), guarded by an `assert` that only Debug builds and the tests compile in. It is the same contract as `Links` (`links.h:71`), so it is consistent; the products' wiring (cluster's task) has to honor it. | `seeds.h:114`, `:222-227` | None here; the cluster wiring's review checks it. |
| N4 | no | nit | Under spoofing, an announcement with a genuine node's source address and sender replaces that seed's ports (`e.seed.services = ...`), so a spoofer can send its next dial to a wrong port and drive its backoff up to 60 s before it joins. Joined seeds are not affected. This is inside the trusted-cluster threat model `seeds.md` states. | `seeds.h:349` | Optional: with F1's target key, a changed port set is a new target and leaves the genuine one's backoff alone. |
| N5 | no | nit | An `on_dial` that throws leaves its entry `dialing` forever: never swept, never offered again. | `seeds.h:350-353`, `:363` | Document that `on_dial` must not throw, or set the entry idle when it does. |
| N6 | no | nit | Timing upper bounds can flake on a loaded runner: `rate` asserts every gap is at most 210 ms and at least 12 datagrams in 1.5 s; `forged` asserts at least 3 impostor dials. | `seeds_test.cc:519-522`, `:429` | Keep the lower bounds (the rate limit) and report the upper ones, or widen them. |

Answers to the lead's questions:

1. Decoder safety: safe. A size check guards every read, allocation is bounded by the 512-byte
   input, and 22.5M fuzz runs under ASan and UBSan found nothing.
2. Seed-only: a forged announcement never adds a member. The library only offers `Seed`s, and the
   caller's TLS hello against its pins decides. A dial goes to the datagram's source address,
   which a forger can spoof. Starvation: yes, through F1.
3. Lifetimes: correct under the stated contract. `stop()` cancels the timer and closes both
   sockets; every handler checks `stopping_` first; `Pending` counts in-flight handlers until
   `drained()`. A node reported lost after `joined` is dialed again once its backoff passes (the
   `forged` case covers it). Destroying before draining is N3.
4. Portability: exit 77 is right for sandboxes, but the skip decision depends on the code under
   test and CI does not require multicast, so a Linux or FreeBSD failure would read as a skip (F2).

## Not run

- Any Linux or FreeBSD run: CI was queued, and no VMs were allowed. Whether Linux `lo` carries
  multicast, and whether three `SO_REUSEPORT` receivers each get every datagram there, is unproven
  until F2's CI evidence.
- IPv6 groups (interface by name): no test and no probe covers them.
- No further fuzzing past the 300 s job; the TSan build of these cases.
- Xapiand's and Detent's wiring (the cluster quest, and Detent's own).
