# unpin-after-kick review: correctness

Commit: strata:3adfdc274867ae6fa843bccd3cb0a83a3b55c2c3
Verdict: CHANGES
Date: 2026-11-12

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

None passed yet: at review time 261112-051834-5fd5e6 (TSan), 261112-051837-327568 (ASan+UBSan),
261112-051840-b0a9ca (A1 fails-without), 261112-051843-666637 (A2 fails-without) and
261112-051846-b357a7 (build-test) were all queued. I read their commands (`campaign job show`) and
they test the claims: the TSan and ASan jobs run all of catalog_teardown_test (with
last_unpin_under_shutdown) and catalog_sweep_test; the A1 mutant swaps in ab0e26a6's catalog.hh by
`-iquote`, which works because catalog.hh is included only as `"strata/core/catalog.hh"`
(test/catalog_world.hh:7, test/catalog_s8_test.cc:14), and requires an ASan heap-use-after-free;
the A2 mutant deletes the unpin's over_soft() line and requires soft_caps_post's
`hot_indexes() == 1` to fail. They run at da983792, whose code equals 3adfdc27 (docs only differ).
A1 to A3 stay unproven until they pass and CI is green.

## What I checked myself

- The unpin (catalog.hh:1410-1439) by reading. Its compare-exchange is its last touch when it does
  not close; on a marked `{1, open/owed}` it goes to `{0, closing}` in the same exchange, and the
  release_due, note and post that follow run while the slot is closing, so `occupied_` stays above
  zero until run_close and nothing can complete shutdown before the post. over_soft() runs before
  the exchange, while the pin holds the slot. Clean.
- The word (catalog.hh:81-109). Eight states, 0 to 7, fit three bits (static_assert). `state()` and
  `with_state()` mask 7; `shut` is bit 39, between state (36-38) and pins (40-63), so `pins()`,
  `full()`, `add_pin`, `freq`, `gen`, `with_pins` (keeps the low 40 bits) and `next_gen` are
  unaffected. No other decoder of the word exists in include/ or test/. `make()` drops `shut`, and
  is used only on the way to `free` (2284, 2334) and from `free` to `opening` (1366); every other
  transition uses with_state/with_pins/next_gen and keeps it.
- Shutdown interleavings, by reading. Mark and unpin are ordered by the word: a mark that lands
  first fails the unpin's exchange, which reloads and closes; an unpin that lands first fails the
  mark's exchange (2372), which reloads and closes `{0, open}`. Only one of them can move the slot
  to `closing` (each needs open/owed). Two pins: only the exchange that sees `pins == 1` is last. A
  pin after shutdown began is refused (1225); one that passed the check earlier either meets a marked
  slot (`shut` is sticky) or a transition whose end kicks (1694, 1787, 1934; a demotion that hands
  over to a wake ends in the wake's kick). ABA: the unpin holds a pin, so the slot cannot be freed
  and its gen cannot move; a stale mark exchange fails on gen. join_shutdown's kicks run in the
  awaiter's await_ready, and a delivery before await_suspend resumes inline, so they are sequenced
  before the owner can resume.
- Probe 261112-053657-c4635c (`.scratch/review-unpin-after-kick/probe_miss.sh`, patching
  `probe_miss.cc` into catalog_teardown_test, ASan): a synchronous try_pin miss passes the shutdown
  check, shutdown begins at its `pin.miss` hook, and its thread stalls in the open's post until the
  host destroys the catalog. Result: `heap-use-after-free` in `over_soft()` catalog.hh:1443, called
  from `acquire_impl` catalog.hh:1383. See F1.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | defect | The miss path keeps the bug's shape: `post(run_open); over_soft();` (catalog.hh:1381-1382). A synchronous miss (try_pin, or a pin_awaiter whose token is already requested, catalog.hh:460) that B1 counted before shutdown began posts the open; the open ends `{0, open}`, its kick posts the shutdown close, the close completes shutdown, the host destroys the catalog, and over_soft() then reads it. R3 checked this function's refusal branch, not this tail. Pre-existing, not introduced, but it is the race the lead asked about and Xapiand passes cancel tokens to its pins (pool.cc:749). | Probe 261112-053657-c4635c: ASan heap-use-after-free at catalog.hh:1443 from acquire_impl catalog.hh:1383 | Run over_soft() before the post (`if (post_open) { over_soft(); post(...); }`): the slot is `opening` and counted, so the catalog cannot end before it. Add the probe as a test (`catalog_teardown_test` mode) with a `--fails-without` job at the current code. |
| F2 | yes | gap | No test forces the interleavings R1 names. last_unpin_under_shutdown marks before the unpin even loads the word; nothing makes the mark land between the unpin's load and its exchange (the retry that sees `shut`), nor the last unpin land between shutdown_slot's load and its mark exchange (the retry that closes `{0, open}`). The schedule sim injects at `unpin` but never starts shutdown there. A slip in either retry hangs shutdown or closes twice. | catalog.hh:1413-1430, 2371-2374; test/catalog_test.cc:641-664, 906-915 | Two deterministic tests: (a) the `unpin` hook (after the load) begins shutdown on that thread; check the exchange retries, shutdown completes, the slot is free and open-to-closing is counted once; (b) a hook in shutdown_slot's pinned branch before the mark exchange releases the last pin; check the mark retries and closes. Each with a mutant job (close decided from the first load; mark without the retry) that fails. |
| N1 | no | nit | unpin swallows an exception from begin_shutdown_close (catalog.hh:1434-1437): a failed post leaves `{0, closing}` with no close, and shutdown waits forever without a word. kick() already did the same before this change. | catalog.hh:1434, 2351-2357 | Leave, or count it in stats so a hang is visible. |
| N2 | no | nit | Transition ends still kick by an acquire load of `shutting_` after their word exchange, against join_shutdown's exchange then acquire load of the word: formally short of seq_cst (store-buffer), ordered in practice by the RMWs on x86 and ARMv8. R1 retired only the unpin's instance. Pre-existing. | catalog.hh:2352, 2401-2403 | Leave, or make the two loads seq_cst. |
| N3 | no | nit | unpin's fetch_sub became a load and a compare-exchange loop on the hot path; the pin_smoke A/B (261112-051903-f5de71, 261112-051905-9ea016) is pending. | catalog.hh:1413-1430 | Read the A/B before landing. |

## Not run

No TSan or ASan run of the quest's tests by me (their jobs were queued); no Linux; no stress of
unpin against shutdown with several workers per slot; the pin_smoke A/B; Xapiand's test_node at a
re-pinned strata.
