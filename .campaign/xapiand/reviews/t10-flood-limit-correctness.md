# t10-flood-limit review: correctness

Commit: Xapiand:e817fdc2be89 (code at Xapiand:7b6768c3e; src and tests identical since Xapiand:e78e3a766)
Verdict: CHANGES
Date: 2026-11-17

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- 261117-015256-6a526c: `xapiand_test_lua` (996 checks) and `xapiand_test_loop_yield` (846) at code-identical
  fbd5d8ab4. The call test prints "41 resumes after its entry, 41 of them heavy"; the RESTORE chunks print `lllHHHHH`.
- 261117-015216-5e1452 (mutant: no hop): fails `heavy >= 20` and `heavy == during` (0 of 40 heavy). A1's test is live.
- 261117-015303-d945e6: the full ctest at fbd5d8ab4.
- FreeBSD CI run 44059619013 at fbd5d8ab4 (same code), artifact `t10_lua.log`: one flood call alone 20.9 ms, limit
  62.7 ms, light GET p99 30.95 ms and max 35.99 ms beside 16 flooders. That is A3's row passing with a 2x margin.
- The exclusive sweeps afc6c2/9ae194 and the cost rows, as the dev note reports them.

## What I checked myself

Scripts are in `.scratch/review-t10-flood-limit-correctness/` (home checkout).

1. Exit paths, by reading (`src/lua/processor.cc:892-902`, task:18eb516 `task.hh`).
   - `LightOnExit` is declared after the hop and before any throw site, so every body exit runs it: an error, a
     condemnation (`throw_if_condemned`), a cancelled `resident_lock` wait, or a checkpoint's `cancelled`.
   - `held` is declared after it and is destroyed first. `async_mutex::unlock` posts the next waiter through its
     captured reference (`resume_on`), so no other call runs inline inside this scope.
   - Hook yields come back through `budget::checkpoint`, which posts heavy in a heavy scope. The tests cover this.
   - A request that is already heavy has `light == false` and keeps its lane. The tests cover this.
   - Lua exposes only `print` and `condemned` (`processor.cc:685-686`), so a script cannot call back into a route
     or nest a call.
   - Frames are destroyed only at teardown (`root_list::destroy_all`). The destructor's `enter_light` there touches
     only the teardown scope.
   - When there is no scheduler (`sync_wait` in `prepare_plain`), `task::current()` is empty, so no hop happens.
2. Nothing is held while the request waits for the heavy lane (`handler.cc:449-476`, `schema.cc:2760-2912`,
   `handler.cc:2940-2985`).
   - Every `lock_shard` in `Schema::index` is scoped and released before `call_script`.
   - The write path is optimistic, with version-conflict retries and no lock around `prepare`.
   - RESTORE prepares scripted objects through `prepare_inline` outside any turn.
   - The hop comes before `resident_lock`.
   - The lanes rule guarantees one heavy turn per pass, so neither lane starves the other.
3. The cap counts work, not time spent queued. `note()` charges units (`processor.cc:130-140`). `slice_work` resets
   at each hook yield (`:196`). The budget's slice starts at each resume (`running_on` sets `slice_start`), so the
   time spent queued for the heavy lane is not counted.
4. RESTORE's fast lane still holds. An unscripted RESTORE never enters `Processor`. 6a526c and d945e6 pass at this
   code.
5. The limit (2+1) x one call. The hop count k = 2 comes from reading the code, and no test pins it. Two things
   make it acceptable:
   - If a hop were added, the limit would become too tight and fail loudly; it cannot quietly become too loose.
   - FreeBSD measures 1.5 stretches and macOS 1.75, against a limit of 3.
   I judge it honest.
6. Probe P1 (`scripted_restore.py`, `run_p1.sh`, job 261117-034526-66767d) timed RESTOREs of objects that each carry
   `_script`, so each object makes one call and one hop. One reactor, fix against master's merge base 9ae4f7957,
   A B B A. The fix's first run completed every row, with values checked:

   | Row | Scripted | Plain |
   |---|---|---|
   | Idle, 2000 objects | 1826 ms | 320 ms |
   | Beside 8 GET-only clients | 2452 ms | 343 ms |
   | Beside 2 flooders, 200 objects | 4954 ms (40 docs/s) | 599 ms (334 docs/s) |

   Beside the flooders a scripted object costs about one flood stretch (24.8 ms each).

   The master run did not finish its scripted-beside-flooders row. The RESTORE and both flooders' PUTs timed out at
   300 s, and the job failed (exit 1). I lost the node log because the probe deleted its directory.
7. Probe P2 (`hang_probe.py`, `run_p2.sh`, job 261117-042554-2e8d5a, submitted, still queued at handback) repeats
   that row twice per binary. It records the time to finish, and on a stall it samples the node's stacks and keeps
   the node log in `$JOB_OUT`.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | gap | D7's cost is measured only for a one-call request. A request with many calls (a RESTORE or update stream whose objects carry `_script`) re-enters the heavy lane once per object, because the scope is light again after each call. Beside 2 flooders the fix pays one flood stretch per object (40 docs/s against 334 for plain objects). Master's figure is unknown: its run stalled in the same row. There is no master comparison, so the cost the design states ("as a heavy request does") is unproven for this path. | P1 261117-034526-66767d; `processor.cc:892-902` | Read P2 (261117-042554-2e8d5a). If master stalls in this row or is no faster, record that in the dev note's Cost section and the gap closes. If master is clearly faster, add a cost row for a scripted RESTORE (`scripted_restore.py` serves) and decide whether keeping the request heavy across consecutive calls (see F2) is the remedy. |
| F2 | no | nit | `LightOnExit` restores light even when the call's own budget crossed `REQUEST_LIGHT_SLICES` during the call. That overrides "heavy is sticky" until the next spent slice. It is bounded by one slice, and the test intends it (≥20 slices, then light). | `processor.cc:897-901`; `test_lua.cc` the first D7 test | Options: (1) keep it; (2) restore light only when `budget->spent()` is below the light allowance. |
| F3 | no | gap | Master's merge base (9ae4f7957) stopped answering everything (RESTORE and flood PUTs both silent for 300 s) during a scripted RESTORE beside 2 flooders on one reactor. This predates the quest; whether D7 removes it or hides it by timing is unknown. | P1 log, the master run's tracebacks | P2 settles whether it reproduces and where it sits (stack samples). If it reproduces, file it on the hunt list. |
| F4 | no | nit | The comment "Compiled and run without a wait between" (`handler.cc:473`) is stale: `operator()` now waits for its heavy turn. Lifetime is safe because the local `shared_ptr` holds the processor. | `handler.cc:473-475` | Reword the comment. |
| F5 | no | nit | There are two `LightOnExit` structs (`search_views.cc:3168`, `processor.cc:897`) for one mechanism. | the two lines | Optionally one helper. |
| F6 | no | gap | Pre-existing: TSan reports a race in `test_loop_yield`'s `count_delete` (reads `counted_thread`, `tests/test_loop_yield.cc:78`, against the write at `:1239`). It failed CI run 44056420626 at 37eda4ed0 and passed at fbd5d8ab4 with the same code. That file is not in this quest's diff. | the run's TSan report | Hunt list. Make `counted_thread` atomic or set it before `counting`, with release ordering. |

## Not run

- P2's result. It was queued at handback; F1 and F3 turn on it.
- No sanitizer build: no lifetime or race question about the change needed one.
- FreeBSD CI at e817fdc2b was still running. I relied on the code-identical run 44059619013.
- No multi-reactor run of P1.
