# lifecycle-adoption review: design

Commit: Xapiand:0669d9415 (`docs/design/lifecycle-adoption.md`, branch design/lifecycle-adoption)
Verdict: CHANGES
Date: 2026-11-19

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

The split is sound: library quests first, signals before the driver, state by reference before
the driver, and the serialization of the core files (D6) holds up against the alternatives it
names. Four blocking findings remain, all text fixes. Each is an interleaving or contract that
exists only on the shutdown path, which the plan's criteria leave to TSan timing or leave out. One
round of text fixes settles them.

## Evidence I relied on

None. This is a plan, and nothing is built. Inputs read: the plan at Xapiand:0669d9415 (src is
identical to Xapiand:dc340b302), cpp-libraries:04a75c0 `docs/server-composition.md`, task:b7e2449
`docs/design/service-shape.md`, `lifecycle.hh` and `task_timers.hh`, reactor:01ea267 `reactor.h`,
PLAN.md D62, D71-D77, D132-D140, hunt rows 105, 117, 198, 200, 213, 214 and 219,
`reviews/task-inbox-first-correctness.md` F3, F6 and F7, and Xapiand's `src/manager.cc` (signals,
`shutdown_sig`, `join()`), `src/main.cc` (`sig_handler`, `setup_signal_handlers`),
`src/xapiand_fsyncher.h` and `src/database/wal.cc:95-125`.

## What I checked myself

Reading only, no probe jobs: the probes this plan needs are its own criteria, rewritten as below.

1. **Ordering.** Every intermediate state is a working server, with the qualifications in F1, F6
   and F7. After 6, the first signal is still today's path (listeners open, wait on
   `http_clients`); that matches the plan's Limits section and ships. 8 must precede 9, because
   bumping http to http-limits removes the setters, so the pin bump forces `Limits`.
2. **Size and parallelism.** The seven-quest chain is mostly forced: 6, 8, 9 and 11 all rewrite
   `main.cc` and `manager.cc` in overlapping regions. Two levers remain (F10, F11). Library quests
   1 and 4 can merge (same repo, same `after`, one pin bump for Xapiand); 2 and 3 cannot (two
   repos, and 3 needs 2).
3. **Risk calls.** 6, 8, 9, 10 and 11 are rated high for consequence, correctly. 2 and 1 are
   under-rated (F4, F9), and so is 5 (F8). None is over-rated.
4. **Criteria.** The count and grep checks (5 A2, 7 A1, 9 A6, 11 A3) sit beside behavior tests,
   so they are acceptable for refactors. The concurrency criteria (6 A4, 9 A7, 2 A3, 1 A5) lean
   on TSan timing where the proof standard asks for deterministic interleavings (F2, F4, F9).
5. **Gaps against the hunt row and the note.** Hunt row 200 is fully covered; its D138
   precondition (the detent-agent column) has already landed at cpp-libraries:04a75c0. The note's
   Xapiand column is covered except where F12 and F13 say. D2 and D5 go beyond the note; both are
   justified but belong in it (F14).

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | defect | Quest 6's contract contradicts itself. D3 maps hurry onto today's "insisted", but `join()`'s `insisted()` abandons Stop 3's index wait (no commit, `_Exit(EX_TEMPFAIL)`). The user-visible line says a second signal "still commits". It also says the second signal used to exit "at once", but today the third signal (`-EX_SOFTWARE`) does that; the second gives an immediate `join()`. The changelog and scenario 2 inherit this ambiguity. | `manager.cc:1041-1042,1140-1148` (Stop 3 abandons on `insisted()`), `:647-660` (the third signal); plan D3 and quest 6's user-visible text | State what hurry does at each Stop in 6. Either Stop 3 stops consulting `insisted()`, so indexes commit as D71 says, or the changelog says a hurry during Stop 3 leaves the commit to WAL replay. Add a criterion: a second signal while a slow commit holds Stop 3 commits (or exits as stated), with a pinned exit status, as a test with a hook holding Stop 3. |
| F2 | yes | gap | The double finish is named but not designed. In 6 the signal thread's flush runs beside `join()`'s Stop 4-5; in 9 it runs beside lifecycle's end-of-path flush. D140 and service-shape D16 require `flush()` to be idempotent and thread-safe. The plan never says that an idempotent flush must *wait for* a flush already in progress. If the second caller returns early, the signal thread restores `SIG_DFL` and raises before the WAL writers have joined, and acknowledged writes are lost. Two threads joining one `std::thread` is also undefined. 6 A4 is TSan timing only. | `manager.cc:1155-1185` (Stop 4-5 `finish`/`join`); `lifecycle.hh:276` (flushes run last first); plan Limits bullet 3 | Name one flush function per owner (WAL writers, then fsyncher, in that order), guarded by a once-latch whose later callers block until the first completes. Replace 6 A4 with a deterministic test: a hook holds Stop 4 mid-join, the third signal arrives, and the test asserts one finish, that the raise comes only after the join completes, and that a restart reads every write. Run it under TSan. Reuse it in 9 for the end-of-path flush. |
| F3 | yes | gap | Quest 9 makes HTTP teardown run beside a live control loop, and the plan does not examine that. Today the control loop is stopped during Stop 1. In 9 the loop stays live (D7), so registry timers, autocommit and housekeeping may resume or post work onto HTTP loop schedulers while a drain thread runs their `close()` and `destroy_roots()`. Hunt row 105 F2 ("must `release()` after the loop joins and after every `arm_remote` producer returned") is exactly this hazard, yet the plan defers it to "a quest after 9". I could not settle whether task's `post_call` racing `close()` is safe (service-shape D10 covers cancel, not post). | `manager.cc:1060,1094-1101` (control loop stopped, then `close`/`destroy_roots`); plan quest 9 outcome, D7, Left out 1; hunt.md:105 | Name the interleaving in 9. Either show that no control-loop producer can reach an HTTP scheduler after its loops join (cite the task test), or close row 105 F2 inside 9. Add a deterministic test: a control-loop task posts to an HTTP loop's scheduler while the drain thread runs `close()` and `destroy_roots()`, under ASan and TSan. |
| F4 | yes | gap | The dump moves to the signal thread (6) with no lifetime or thread-safety story. It reads `__repr__()`, `database_pool->dump_databases()` and `schemas->dump_schemas()` from a thread that is not the loop, while requests run and while `join()` resets `database_pool` and `schemas`. A SIGUSR1 during teardown would be a use-after-free, and a crash in Stop 3 drops the queued WAL, which D71 forbids. 6 A2 tests only a wedged loop. | `manager.cc:595-600` (the dump), `:1206-1212` (resets); `lifecycle.hh:310-311,386-387` (dump on the signal thread) | State the dump's lifetime rule (for example, the dump callback is cleared or latched off before teardown, and the dumped objects are safe to read concurrently). Add criteria: a dump with requests in flight under TSan, and a SIGUSR1 delivered during a Stop 3 held by a hook, which exits normally with writes intact. |
| F5 | no | gap | Quest 6 omits two pieces of today's signal behavior. A SIGTERM before the manager exists (start-up) exits `EX_SOFTWARE` today, and SIGTERM/SIGINT point stdin at `/dev/null`. With `stop_signals`, an early stop must be observed when the stop path subscribes (the token is level-triggered, so it is fine if checked). `stop_signals` must also be built after `detach()`'s fork. | `main.cc:236-262` | Add criteria for 6: a SIGTERM during start-up stops the node, and stdin is redirected on stop (service-shape's `cancel_callback`). Name the placement after `detach()`. |
| F6 | no | defect | Quest 8 silently carries a shutdown change. http-limits removes `set_shutdown_grace` (C1), but `join()` sets the adaptive grace through it. So 8, not 9, must delete `http_shutdown_grace(left)` and go to `Limits`' fixed 5 s. Its tests (`test_node.cc:862,1061` per the note) change with it, and `tests/test_node.cc` is not in 8's files. | `manager.cc:1078`; note appendix row "Shutdown timing" | Name the fixed grace in 8's outcome. Add `tests/test_node.cc` to 8's set (no collision, since 7 and 9 are serial with it). |
| F7 | no | gap | D2's "`stop()` keeps running them in order" is ambiguous for apps that have not adopted, which is Xapiand between 2 and 9, plus Prism and Detent. Today's `stop()` cancels first (`abort_all`) and never waits. If the legacy `stop()` runs `wait_requests`, or stops accepting before cancelling with a wait, every app's shutdown changes. | reactor `reactor.h:792-814`; plan D2 | Say that the legacy `stop()` is `stop_accepting`, `cancel_requests`, grace, then `force_connections`, with no wait. Add a reactor criterion: legacy `stop()` still cancels an in-flight request at once (a test that fails if `wait_requests` is inserted). |
| F8 | no | gap | Quest 5 claims "behavior unchanged" while replacing `drain_control(tasks, "control", deadline)`, which is bounded, with `group::join`, which wakes at zero and has no bound. A control task that never ends would now hang `join()` where today it hits the deadline. 5 is rated normal and has no criterion on the stop path. | `manager.cc:1061,1117`; service-shape D5 | Add a criterion: a control task that ignores its stop still lets the node exit within `shutdown_write_bound` (a test with a hook). Alternatively, keep a bounded join until 9. |
| F9 | no | gap | `wait_requests` (2 A3) and `fire_pending()` (1 A5) are concurrency primitives that later quests rely on for shutdown and durability, and both are tested by TSan timing. `wait_requests` has a lost-wakeup window between setting the stopping flag and the gate's last leave. `fire_pending` races a watch task's due fire and a late `touch`. Both quests are under-rated on these points. | plan D2, D5, Limits bullet 4; `task_timers.hh` `watch`/`touch` | Add deterministic interleaving tests: the last leave lands between the flag store and the wait; and `fire_pending` runs while a key is mid-fire on the loop, then a touch arrives after `stop`. Keep TSan as the gate. |
| F10 | no | nit | Parallelism: fsyncher-on-task (10) needs only quests 1 and 5 if its `debounce` runs on the control loop's group, with `fire_pending` in today's Stop 5. That takes it off the 9 chain and drops the scheduler pin, which unblocks Detent's archive (hunt row 200, D73), two quests sooner. The cost is a `wal.cc` collision with 7. | plan Order; hunt.md:200 | Optional reorder: 1 + 5, then 10, ahead of or beside 6. Owner's call on chain length versus pin removal. |
| F11 | no | nit | Quest 11 bundles a high-risk process step (user-visible `--detach` and second-instance semantics, the data-directory `flock`) with mechanical moves (node identity, metrics, deleting `manager.*`). Splitting it puts the high-risk review on about 400 lines instead of 1,100. Both halves stay serial. | plan quest 11 | Optional: process-step (high), then node-and-metrics (normal). |
| F12 | no | gap | The Registry's exclusive data-directory `flock` (11) also binds dump and restore, which run the Registry under `lifecycle` (9). A dump or restore against a live node's data directory then becomes a refusal. This is user-visible and unlisted. It narrows behavior, so it is the owner's call. | plan quests 9 and 11; note Deviations "No data-directory exclusion" | Options: (a) refuse with an error naming the holder's pid, and list it in 11's user-visible text and changelog (recommended, since a concurrent writer is unsafe today); (b) give the tools a shared mode; (c) exempt the tools. |
| F13 | no | gap | The note's Xapiand items are missing from quest 8's user-visible list and criteria. Plain HTTP gains the 30 s header and 60 s inactivity timeouts (Settled 1), and TLS becomes `strict` (C2), which narrows TLS 1.2 and turns resumption off. Quest 9 also omits D76's "bare FLUSH flushes databases and clients" and D71's keep-alive refusal at Xapiand level. | note Settled 1, C2; PLAN D76, D71 | Add these to 8's user-visible text and changelog, with an A-row for a plain request trickling past 60 s. Extend 9 A1 with a keep-alive request refused after the first signal, and 9 A2 with bare FLUSH. |
| F14 | no | nit | D2's stop phases and D5's `throttle`/`fire_pending` are library contracts that Detent's and Prism's adoptions will use too (D74: mirror). They appear only in this Xapiand plan. | note "Adoption order" step 2; D74 | Fold D2 into the note's reactor contract (and D5 into service-shape) when 2 and 1 land. |
| F15 | no | defect | The file sets and `after` lists disagree with the plan's own text. Quest 1 lacks `task.hh`, though F6's `queued()` and `post_call` live there (`task.hh:474,486`). Quest 5's `after` omits restore-exit-status, light-share and stopword-lists, which the collisions table says land first. 5's set includes `tests/test_control_plane.cc` and `CMakeLists.txt`, which stopword-lists holds. | plan quest 1 Files, quest 5 header, Collisions table | Add `task.hh` to 1, and add the three `after` entries to 5. |
| F16 | no | nit | The fsyncher's `fire` carries `full_fsync`, but `debounce<int>`'s `fire(key)` has no argument. The key must carry the flag, or the pending set must keep it. `throttle` also needs per-key state after the fire, which `debounce` now erases. | `wal.cc:108,117`; `task_timers.hh` `watch` | Name the key type and where throttle state lives (and how it is bounded) in D5. |
| F17 | no | nit | 6 A1 restarts after a kill, so the page cache survives and the fsyncher half of the third-signal flush goes unproven. POSIX also merges back-to-back signals (service-shape Limits). | service-shape Limits | Space the three signals in 6 A1. Leave the fsync proof to 10 A1 and A3, and say so. |
| F18 | no | nit | Hunt row 117 (Detent's PeriodicTimer removal waits on `manager.cc`, so reply to detent-lead when it lands) and its "Wheel N1" are not mapped to a quest. | hunt.md:117 | Name 5 as closing row 117 and carrying the reply. |

## Not run

- No probe jobs: nothing is built, and every probe above is a criterion that the owning quest
  runs.
- I did not check whether task's `post_call` racing `asio_scheduler::close()` is safe (F3). A
  recursive search of `src/` for `arm_remote` was denied by the permission classifier: it would
  have crossed `src/xapian/**`. I did not retry it.
- Not checked: whether `tls.cc` already applies a `strict`-equivalent policy (F13's TLS half), the
  `opts.cc` and `search_views.cc` line citations, the size estimates, and Detent's
  `daemon.cc`/`daemon.hh`.

## Re-check

Commit: Xapiand:cf5c20817
Verdict: APPROVE
Date: 2026-11-19

I read `git diff 0669d9415 cf5c20817 -- docs/design/lifecycle-adoption.md` for the four blockers,
and task:b7e2449 `task_asio.hh:77-115` for D9's claim. No jobs were run; the plan is still unbuilt.

| ID | Status | Where it is fixed |
|---|---|---|
| F1 | fixed | D3's table separates each Stop. Hurry is no longer consulted in Stop 3, so indexes commit (in 6 within the bound). The third signal's current exit 70 is spelled out. 6 A2 holds Stop 3, sends a second SIGTERM, and asserts exit 0 with the write in the commit; its mutant keeps `insisted()`. |
| F2 | fixed | D8 gives each owner one latched flush: a mutex held for the whole run, with `done` set last. A later caller blocks, and only the holder joins. The fsyncher is registered first, so the WAL flush runs first. Registrations die before the resets and wait out a run in progress. 6 A3 holds Stop 4 between two writers' joins, with an `exchange`-latch mutant under TSan. 9 A7 reruns it at the end-of-path flush. Limits names the cost: a WAL writer stuck on its disk is unkillable by SIGTERM, as in today's `join()`. |
| F3 | fixed | D9 names the producers (a turn handoff, a chore resume, a cross-thread cancel) and closes hunt row 105 F2 inside 9. The claim checks out at `task_asio.hh:77-97`: `post` and `post_call` drop work when closed, and the queued lambda re-checks `closed()`. Phase 4 runs only after the control loop joined. 9 A6 forces both handoffs at the two hook points, under ASan and TSan. |
| F4 | fixed | D10's `dump_target` is a mutex plus pointers, cleared after Stop 5 and before the resets (in 9, when `lifecycle` returns). The concurrent reads it relies on are cited. 6 A4 covers a wedged loop, requests in flight under TSan, and a dump during a held Stop 3; 9 A7 covers the Registry drain. Limits names the cost: a dump blocked on a lock delays later signals. |

The designer flagged two choices:

1. **Quest 8 drops the adaptive grace (F6).** This is the right reading. D77 fixes the 5 s grace,
   and the note deletes the reserve and the adaptive grace. At the default 35 s bound the stop
   behaves as today. Only a `--shutdown-timeout-ms` under about 15 s changes, and that option is
   new in the same quest, so no existing deployment sees a difference.
2. **Quest 9 leaves commits unbounded.** This is consistent with D71: its bound covers requests
   in flight, and indexes "still commit". It is also consistent with service-shape D7 and D9 (one
   bound for requests; durable work unbounded). It is not consistent with the note, whose
   Deviations row says "`--shutdown-timeout-ms` bounds every stop" (Settled 3), and D77 names
   the option as the shutdown bound. It is a user-visible change, and the owner must decide it.
   It also has a durability edge. Today a commit stalled past the bound exits 75 with the WAL
   flushed (Stop 4 runs after Stop 3 gives up). In 9 the WAL flush waits behind the stalled
   Registry drain. A supervisor that sends one SIGTERM and then SIGKILL (systemd's
   TimeoutStopSec) never sends the third signal, so it would lose the queued WAL writes. That
   happens only when the commit is stuck while the WAL disk is healthy, for example a lock bug.
   Options:
   - (a) Accept unbounded commits. State that the option bounds requests, and correct the note's
     Deviations row and Settled 3.
   - (b) The Registry's commit wait takes `b.abandon`. At abandon it leaves the remaining commits
     to WAL replay; `flush()` still runs, and the node exits 75 as today.
   - (c) Bound the commit with its own option.

   I recommend (b). It keeps today's exit-75 contract and the note's wording, and the WAL is
   flushed before any supervisor's SIGKILL. D71 still holds for every commit that finishes within
   the bound.

No new blocking findings. F5-F18 were not re-checked here.
