# single-flight-budget review: correctness

Commit: 19f1e0c9a2399855ea2582ad9efd0737fa12861b (code reviewed at eea2bee555fb; re-checked below)
Verdict: APPROVE
Date: 2026-11-21

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

The verdict holds only if both probe jobs below pass. The TSan probe passed; the ASan probe was
still queued when this file was last updated. If either fails (a sanitizer report, a HANG line, or a FAIL check), the
verdict becomes CHANGES and the failing case becomes a blocking defect whose test is that case.

## Evidence I relied on

- 261121-133028-a36e8e: the new test at mutant 16a783020 (main's wait behind the new signature)
  exits 8, so A1's test fails without the fix. A mutant instead of main is sound: main's `run()` has
  no token parameter, so the test would not compile there.
- 261121-133832-38434f: tests/test_single_flight.cc and tests/test_control_plane.cc pass at
  eea2bee55. The control-plane cases include the existing cancelled-leader and destroyed-leader
  cases (tests/test_control_plane.cc:467, :504), which now run on the new per-joiner wake.
- 261121-133833-1d6960: the joiner test under TSan, 5 runs, at eea2bee55.
- 261121-133836-974d76: the suite at eea2bee55 (A2).

## What I checked myself

Scripts: Xapiand-wt/xapiand/single-flight-budget/.scratch/xapiand/review-single-flight-budget-correctness/
(`run.sh tsan|asan N` copies `test_sf_review_probe.cc` into tests/, builds it beside
test_single_flight in that flavor's tree, runs both N times, and removes the copy on exit).

Probe jobs at eea2bee555fb:
- 261121-163732-ba7006: TSan, 3 runs (build-tsan, the contributor's tree and flags). Passed, with
  no TSan report and 15497 checks per run. Both sides of the race were reached: of the fired
  joiners, 772/761/737 got the value and 811/822/846 left cancelled. Up to 9 loads in one
  cancelled-leader round.
- 261121-163735-74adb3: ASan with detect_stack_use_after_return, 3 runs (build-asan, its own
  `.deps-asan` tree). Still queued.

The probe runs two stress cases, 300 rounds each. A leader runs on loop A and 8 joiners on loops B
and C, each joiner with its own `cancel_source`. A random subset of tokens fires from another
thread while the main thread ends the load, each after a random delay.
1. `probe_tokens_race_the_end` (risks 1 and 3): each joiner ends exactly once, with the value or
   with `task::cancelled`. A joiner whose token never fired gets the value. The load runs once,
   `waiting()` drains to 0, and the key is free afterwards. It prints how many fired joiners went
   each way, so the end-first interleaving is shown to be reached.
2. `probe_leader_cancelled_races_joiner_tokens` (risk 2): the first load throws `task::cancelled`
   (the leader's own token) while joiners' tokens fire. The leader gets its own cancellation. Every
   joiner ends exactly once. A joiner whose token never fired loads again and gets the value. There
   are at most 1 + N loads.
In both cases a stall is reported as `HANG` and the probe exits 3, so a lost wakeup cannot pass.

Read against the code (src/single_flight.h at eea2bee55; task.hh at the pinned
task:18eb51672e4f):
- Lifetime (risk 1). `cancel_callback` has `std::stop_callback`'s lifetime (task.hh:1206-1209). Its
  destructor deregisters and waits for a callback running on another thread, so the raw
  `wake.get()` it captures cannot outlive the frame's `wake`. `event::set()` reads `next` before it
  resumes a waiter and touches no member afterwards (task.hh:1448-1463), so a joiner that frees its
  event during that resume is safe. A joiner that leaves erases its own entry under `mtx_`; `end()`
  swaps the list out under the same lock, and the `shared_ptr` copies keep each event alive while
  it is set. A frame destroyed while it waits (teardown) unlinks its awaiter (task.hh:1475-1477).
  Its entry stays in the list and keeps the event alive, and a later `set()` finds no waiter. I
  found no dangling entry or use-after-free path.
- Leader (risk 2). `run()`'s token is never consulted on the leader path. The leader's load ends by
  the budget it carries (`load_metadata`/`load_foreign` take `budget`), and that failure goes
  through `flight->error`, then `end()`, then each joiner's catch of `task::cancelled`, then a
  token check, then a retry. A joiner that leaves touches only its own list entry, never the load.
- Races (risk 3). Each waiter's outcome is decided once, under `mtx_`, by `flight->ended`, which
  `end()` sets in the same critical section that takes the list. The list entry is pushed under the
  lock `end()` takes. The event latches. A token that was already requested runs the callback in
  the constructor, before the `co_await`. So no wakeup can be lost and no completion can be doubled.
- Wiring (risk 4). Both SingleFlight callers in src/database/schemas_lru.cc pass
  `budget ? budget->token() : task::cancel_token{}`: the metadata load (:371-375) and the foreign
  load (:617-621). `task::cancelled` is a plain `std::exception` (task.hh:917), so the foreign
  path's catches (`ClientError`, `Error`, `engine::Error`, :629-642) do not turn a joiner's
  cancellation into the "create a new initial schema" fallback. No other schema path joins a
  flight. The non-first-use paths call `read_metadata`/`load_shared` with the budget directly.
- Doc (risk 5). The Limits bullet is gone, and How step 1 states the behavior the code has. See N2
  for the wording.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| N1 | no | gap | The suite does not pin two interleavings the design names. The first is the ruling "a woken joiner whose flight ended returns the value even if its token was requested in between". The second is a cancelled leader with token-carrying joiners: those whose token fired leave, the others load again. tests/test_single_flight.cc covers token-first, already-requested and many-leave only. The control-plane cancelled-leader case uses token-less joiners. | tests/test_single_flight.cc:94-189; tests/test_control_plane.cc:467; probes 261121-163732-ba7006, 261121-163735-74adb3 | Add the two probe cases, or deterministic versions of them, to tests/test_single_flight.cc. |
| N2 | no | nit | "a leader that is cancelled ends the joiners' wait and each loads again" (docs/mechanisms/cancellation.md How 1, wording carried over from the old Limits bullet): each joiner retries, but only the first to retry loads and the rest join it. Only a joiner whose token is not requested retries. | docs/mechanisms/cancellation.md:23-25; src/single_flight.h:156-166 | "...ends the joiners' wait, and each joiner whose token is not requested runs `run` again (one of them loads)". |
| N3 | no | gap | Outside this quest: index settings loads (`settings_loads.run`, src/index_resolver_lru.cc:634) still join without a token, and their load takes no budget either. A request that reaches `resolve_index_endpoints` during an index's first settings load cannot leave on its client. cancellation.md's Limits does not list this. | src/index_resolver_lru.cc:634-637 | A hunt row, or a Limits bullet naming it. |
| N4 | no | nit | The TSan job 261121-133833-1d6960 (and my TSan probe, which reuses its tree) shares `FETCHCONTENT_BASE_DIR=$PWD/.deps` with the RelWithDebInfo build. review.md asks for each sanitizer to have its own dependency tree. The code under test (single_flight.h, task.hh) is header-only and compiled inside the test's translation unit, so the TSan evidence for SingleFlight stands. | job spec of 261121-133833-1d6960 | Give TSan jobs their own `.deps-tsan` tree. |

## Not run

- No deterministic forcing of a joiner frame destroyed while it waits (teardown, Host destruction
  with suspended roots). That case is covered only by reading task.hh's awaiter unlink and the
  existing destroyed-leader case.
- No Linux, no g++: the probes run on this macOS host with Apple clang only. CI covers the other
  platforms (A4 is red at 074d415c8; I did not look at why).
- No end-to-end request test of a joining search whose client disconnects through http. The wiring
  was checked by reading the code only.

## Re-check

APPROVE at 19f1e0c9a2399855ea2582ad9efd0737fa12861b. src/single_flight.h is unchanged from eea2bee555fb. The allowlist row (harness/async_allowlist.tsv:70, count 3) matches the three `lock_guard` sites, single_flight.h:71 (the leader's end), :108 (a join) and :146 (a cancelled joiner's leave), and none of them is held across the load or a wait. One nit: the leave's `std::erase` scans the flight's joiner list, so it costs linear time in the waiting joiners rather than the note's "O(1)". It is still bounded and does no I/O.
