# loop-scheduling review: correctness (concurrency)

Commit: 1317dd6850f17433f0b76fc0afd5ce576b7313f7
Verdict: CHANGES
Date: 2026-11-13

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

Scope: the loop-scheduling part (Xapiand:93b4d125c, Xapiand:bc5fea438) and the lane wiring in
`src/server/search_views.cc`, `src/engine/host.h`, `src/engine/host_asio.h`, `src/database/handler.cc`.
Xapiand:87f839a95 belongs to x7-loop-fix-c (already reviewed). The lane mechanism in task:18eb516 is taken as
reviewed; only Xapiand's use of it is judged.

## Evidence I relied on

- Gate 261113-042355-86c006 at this commit: build, 39 of 39 tests, async check, exit 0 (asserts-on build, so the
  `Counted` debug path is what the suite runs).
- Mutants at this commit, each `done 0` (the named check fails): k0 261113-042355-ac17b8, no-rearm
  261113-042356-6f67c7, pump-drops-lane 261113-042356-b1cb7d, no-exit-guard 261113-042356-bbdfbb. Their
  4117329cf runs (261113-033318-52f956, -033325-227525, -033332-72bffb, -033338-89cc5a) print the values each
  check reads (waited 4 vs at most 1; next request entered heavy; chunks entered at 8/16/24; lanes llllllll).
- CI at this commit: success (includes the TSan unit tests that failed in run 43419798840).

## What I checked myself

- Lane entry, by reading: every request starts light (fresh budget, `LightOnExit` on every exit including a throw,
  `search_views.cc:3168-3171`); `counted()`'s two hops keep the current lane (`host_asio.h:221`, `:233`); the
  Counted pump resumes each entry in its posted lane (`host_asio.h:179`); chores capture `task::current()` with its
  lane (`pool.cc:96`, `:129`); timers too (`host.h:178`); `SingleFlight` joiners resume through their own captured
  reference via `task::event` (`single_flight.h`), so a heavy leader does not make a light joiner heavy. No Xapiand
  code posts through an explicit `scheduler_ref::of(...)` that would drop the lane on a request path.
- Rearm ordering (risk 2): `rearm_light` (`search_views.cc:1702`) runs before the `when_all` expression creates the
  two lazy children, so no child can observe the old count or scope. The TSan failure's attribution holds: CI run
  43419798840 printed four entries at 4 spent (one per chunk, each rearmed) plus three extra entries at 12/13,
  each right after a 4 entry: 4 + 8 seam slices = 12, a second prepare round of the same chunk, not an unrearmed
  chunk (which would show fewer entries at 4). `FixedRanges(5)` makes one round per chunk; CI TSan at this commit
  passes.
- Races and lifetimes (risk 3): `budget::spent_` and `rearm_light` are touched only by the request's coroutines on
  the loop (prospect's slices check on the calling coroutine, `prospect/shards.hh:294-301`; chore bodies never touch
  the budget). A heavy chore completion now calls `post_demoted` for the Counted pump from a WorkPool thread, which
  task:18eb516 supports via its hop, and test 1 drives that path under CI TSan. `LightOnExit` on a frame destroyed
  at teardown only rewrites a thread-local.
- Probe 261113-043104-06458b (`.scratch/review-loop-scheduling-correctness/rearm_in_read.sh`, exit 0 = all three
  expectations met):
  - S, shipped code with the seam spinning 300 ms after recording, so the index child finishes after the read and
    the join leaves the parent heavy: test 3 passes, every chunk `lllHHHHH`.
  - M, rearm moved into `Request::read_chunk` (where the design note puts it), test as shipped: test 3 passes. The
    mutant survives.
  - K, M plus the 300 ms seam: chunks 2-4 `HHHHHHHH`, `CHECK(first_light)` and `CHECK(lanes_by_count)` fail.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| C1 | yes | gap | Test 3 does not pin the rearm's placement, which is the rule D82's code comment states ("before the children start, since the join goes on in its last child's lane"). In the test the read child always finishes last (150 ms piece gaps), so every join leaves the parent light, and a rearm inside the read survives. In production a fast client makes the index child finish last and heavy; a rearm in the read then leaves every later chunk heavy (task `docs/lanes.md`, "A rearm covers what starts after it"), which is the lifetime classing the design rejected (X4, 0.7x). | Probe 261113-043104-06458b arms M and K | Make the index child finish after the read in test 3 (the probe's seam: after recording, spend slices for about 300 ms), and add the rearm-in-read mutant (`--fails-without --for A4`) next to no-rearm. |
| C2 | yes | defect | The design note prescribes the wrong placement: `docs/design/loop-scheduling.md:108` and `:255` put `rearm_light` "before each body read in `Request::read_chunk`", and A4 test 3's mutant (`:451`) is "no `rearm_light` in `read_chunk`". That is the placement task's own doc says leaves the next chunk heavy; the code is right and the note (A1's artifact) is wrong, so anyone matching the code to the note reintroduces the bug C1's test misses. | `loop-scheduling.md:108`, `:255`, `:451`; `search_views.cc:1698-1702` | State the shipped placement (in `restore_database_view`, before `when_all`) and why. One text edit, no job. |
| C3 | no | nit | The note's D2 (f) and file list say `enter_light()` at `handle`'s entry and on exit; the code has the exit guard only (journal ruling). The note's A4 test list (single-term search vs four 100-hit searches) also differs from the tests built. | `loop-scheduling.md:167-169`, `:254`, `:446-451` | Fold the journal's rulings into the note with C2's edit. |
| C4 | no | gap | No Xapiand test covers the three script budgets' lanes (`search_views.cc:948`, `:1045`, `:1463`); reverting them to `light_slices` 0 would survive the suite. | Read | Optional: one write-with-script case in test 1's shape. |
| C5 | no | gap | A4's "a heavy request still progresses within its bound" has no Xapiand test: under a strict-priority drainer test 1 still passes. The rule lives in task (`heavy_progresses_under_flood`, mutant `strict-priority`) at the pinned task:18eb516; cite it in the evidence table. | Read, test 1's checks | Cite the task test in A4's evidence. |
| C6 | no | nit | The suite runs the asserts-on build only, so the release path (no `Counted`, heavy posts straight to `asio_scheduler::post_heavy`) is covered by task's tests and by the A2/A3 release measurements, not by Xapiand's tests. In Debug a Counted pump with mixed entries goes to the light FIFO and runs its heavy entries there (`host_asio.h:184-196`); Debug only. | `host_asio.h:162-196` | None needed; noted so nobody assumes otherwise. |
| C7 | no | nit | TSan job 261113-042317-eb813e failed on a harness error (ctest found no `bin/xapiand_test_loop_yield`; the build put it under `bin/<config>/`), and 261113-042327-1ee206 was skipped, so neither result is evidence; CI TSan at this commit is. | Job log | Drop or fix the job path; CI already stands for it. |

## Not run

- No TSan run of my own (CI's at this commit passed); no Linux or VM run; no release-build (NDEBUG) run of the
  lane tests.
- Kronuz/http 09b644d (`resume_waiter` heavy path) and task:18eb516 internals were not re-reviewed.
- A2/A3 measurements and the A1 evidence jobs are outside this lens.
