# x-pool review: correctness

Commit: Xapiand:41dba47c5
Verdict: CHANGES
Date: 2026-11-09

APPROVE: lands as is, at this commit's code. CHANGES: lands after each blocking finding is fixed
and confirmed by a job at the fixed commit. REJECT: the approach is wrong; the quest is re-planned.
An APPROVE lists no blocking finding; a CHANGES lists at least one.

## Evidence I relied on

- build-test 261109-014325-59bf18 (ctest 37 of 37, `async_check --strict --owed` clean), RelWithDebInfo with
  `ASSERTS=ON`, so the debug aborts in `~DatabasePool`, `~ShardEndpoint`, `~IndexChores` and the view rule are live
  in `test_pool`, `test_node` and `test_reactor_requests`. It runs every test the Evidence table names (A1-A5,
  A10-A15).
- tsan 261109-014325-aad33e (37 of 37, 0 reports) at this commit, against master's failing
  261108-223749-0e083d (a thread leak of `RunningNode`'s finished, unjoined thread in an abandon child, and
  `test_index_pattern`'s aborted subprocess). Linux VM tsan 261109-014336-74371a.
- Docs E2E 261109-014336-a36d6f (666 of 666, plain and asserts builds, 0 asserts in the node log) and the e2e check
  261109-014325-433dc2.
- The mutant tables (`docs/status/phase7c-x-pool.md` steps 1-3; step 4's x1-x6 in the journal and the worktree's
  `.scratch/mutants.log`). I read each mutant's kill line; I did not rerun them (see Not run).

## What I checked myself

Read `git diff master..41dba47c5` for `src/database/{pool,shard,lock,handler}.*`, `src/engine/strata/*`,
`src/manager.cc`, the tests the Evidence table names, and strata's `core/catalog.hh` at strata:5d742b2 (the pin,
waiter, `deliver`, `abandon`, `cancel_wait`, `unpin`, `join_shutdown`, `maybe_complete_shutdown`, `~catalog`).

Risks, answered:

1. Pin lifetime. An `IndexPin` is a catalog pin plus the pool pointer; the catalog closes or replaces only at zero
   pins, and `~DatabasePool` waits for the shutdown before `catalog_.reset()` (killed mutant m7). The shutdown's
   `Detached` coroutine is resumed inline by whoever ends the last slot; every such resumer either runs in a catalog
   chore (which `~catalog`'s `group_->wait()` drains) or returns from `deliver` without touching the catalog again
   (`unpin` -> `kick` -> `erase_failed`/`finish_close` end at `maybe_complete_shutdown`), so the destructor cannot
   free the catalog under a resumer. A cancelled waiter is unlinked and uncounted by `cancel_wait`, and a destroyed
   one releases a delivered pin in `abandon` (A8's 160-round test, A10). `Shard::reopen` hands the pin back
   (`catalog::reopen` empties it) before re-pinning, so there is no double close. No leak or use after free found.
2. Cancellation. The token reaches every wait in `pin` (both the finishing resident pin and the request pin),
   `pin_chore`, `Shard::ready`, `Shard::reopen` (through `ready`), `lock_shard::pin` and `search_batch`
   (`turn_token()`); `rethrow_pin_failure` lets `task::cancelled` through. The owed loop (`apply_settling`) keeps
   no Xapiand-side state: a settle chore cancelled at teardown leaves the debt in strata, where the next apply
   answers it again. Gap C2: FLUSH's commit waits do not take the token.
3. Shutdown. Control roots are destroyed before Stop 3 (C24); the stop ends at the shared deadline
   (`test_a_registry_that_cannot_close_ends_the_stop_at_the_bound`, bound - 100 ms to bound + 800 ms, passes under
   TSan); `busy()` reports strata's `shutdown_report` (key, state, pins, reason), and `test_join_waits_for_every_pin`
   checks the line says "pinned". Nit C5 on how the node tests check it.
4. Test fixes at 41dba47c5: correct, and they hide no product race. The Walker stores `retry_after` before the
   seq_cst `state` store that `settles()` loads, which orders the read. The zero-cap test only forks; running it
   first keeps the child from inheriting a TSan report count. `RunningNode::join()` joins a thread that has already
   left `run()`; master's report names that finished thread (`Thread T18 (finished)`), and the product's `main()`
   runs `manager->run()` on the main thread (`src/main.cc:817`), so the product has no such thread.
5. Proof. The tests check values: documents read back by id and data, revisions, WAL COMMIT and REPLACE lines,
   slot states, catalog counters with exact values. Each mutant line names a failing check, not a count alone.
   Weak spots are C3 and C4.

Probe: `.scratch/review-x-pool-correctness/missing_index_write.sh`, job 261109-041911-e40276 at 41dba47c5 (slot
`xapiand-x-pool`, its existing `build-e2e`). It sends non-create writes to indexes that do not exist. Rerun it at
the fixed commit to confirm C1:
`campaign job submit --quest x-pool --slot xapiand-x-pool -- bash "$PWD/.scratch/review-x-pool-correctness/missing_index_write.sh"`.
(The job's run also had a `DELETE /x/:foo` row, which answers 501 for an unrelated reason; I removed it from the
script after the run.)

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| C1 | yes | defect | A write that does not create its index, sent to an index that does not exist, now answers 500 "Database has been closed" and logs `ERROR: Dispatching HTTP request` plus `unhandled exception`. It answered 404 before. `lock_shard::pin` catches not_found and leaves the Shard unpinned (`src/database/lock.h:80-85`), so search batches can skip a missing index. But the write paths call `Shard::endpoint()`, which raises `errc::closed` with no pin (`src/database/shard.cc:1275`), and `catch_http_errors` turns `closed` into 500. On master, `take_turn` -> `ensure_open` -> `open_local(create_missing=false)` raised not_found. Reached by `PUT /idx/:key` (metadata, `DB_OPEN[|DB_WRITABLE]`) and by any `DB_OPEN` write through `lock_shard`. `collect_failure` also marks such a use stale. | Job 261109-041911-e40276: `PUT /rv_missing_meta/:foo` and `PUT /rv_missing_meta2/:foo?primary` answer 500 "Database has been closed"; the `GET` of a missing index answers 404. | Have an unpinned use raise not_found when it is used for anything but a batch read. Two ways: `Shard::endpoint()` re-pins, or remembers the not_found and raises it; or keep the not_found catch inside `search_batch` and let every other `lock_shard::pin` raise it. Rerun the probe. |
| C2 | no | gap | A cancelled FLUSH still waits for every turn that is held. `flush()` passes its token only to `pin_chore` (`src/database/pool.cc:788`). `ShardEndpoint::commit` waits for the turn and for a joined flight without it (`src/database/shard.cc:576`, `:583`), up to `DB_TIMEOUT` (30 s) for each turn. The resulting `busy` is caught and the loop moves to the next key (`pool.cc:794`), where a pin that need not wait is granted despite the requested token. A11's test covers only the pin wait. The commit turn's missing token predates this quest (master `shard.cc:683`); FLUSH's cancellation is new here. | code read | Pass the token through `commit(flags, wal, cancel)` to `take_turn`, and check `cancel.requested()` at the top of each `flush` iteration. |
| C3 | no | gap | `test_the_sweep_erases_a_missing_index_negative_entries` checks only an upper bound (gone within 10 s, with a 2 s TTL). An erase that ignores the TTL and runs at once passes. x5 (the TTL ignored, so strata's 10 s default applies) is killed only by the 10 s `until` window, which is close to the TTL itself. | `tests/test_pool.cc:2223-2244` | Also check that the entries are still there at about TTL / 2, and widen the gap between the TTL and the wait. |
| C4 | no | gap | The one-pin-per-key-per-request rule (D3's CR2) is kept by convention. `search_batch` dedups within a batch, but no debug check catches a task that pins a key it already holds. Such a pin would wait for its own pin, with only the request's token to end it, if the index were being replaced. I found no path that does it. | `docs/design/x-pool.md` D3; `src/database/handler.cc:1346-1353` | Optional: a per-task pinned-key count on `TaskCounters`, asserted like `turns`. |
| C5 | no | nit | The node stop tests look for "pinned" anywhere in the log, not in the abandoned index's line. Today only `busy()` prints the word, so the check holds, but it would not notice a report that names the wrong key or pin count. | `tests/test_node.cc` (both `log.find("pinned")` checks) | Match the `"abandon"` key and `1 pin` on the same line. |
| C6 | no | nit | `Shard::ready()` re-pins after a reopen with `use = true` (`pool.cc:690`), so a request that reopens counts two uses for the policy. CT-R31 says a re-pin within the request is not a use. | `src/database/pool.cc:690`, `shard.cc:1313` | Give `pin()` a `use` argument, or re-pin through `catalog::repin`. |
| C7 | no | gap | The step 1-4 mutants ran locally, outside job records, at earlier heads (steps 2 and 3 before the token and step 4 commits). Their kill lines are specific, and the tests still pass at 41dba47c5. The C24 manager-order mutant has no killer (lead's open item, which A10 stands in for). | ledger; journal | None required. Rerunning m1, m2, s6-s8 and x1-x6 as one job at the landing commit would make them evidence. |

## Not run

- I did not rerun the mutants, and did not build or run anything under ASan or TSan (the jobs above cover TSan
  here; ASan moves to `ci:AddressSanitizer`, out of scope).
- I did not run master's binary for C1. Master's 404 comes from reading its code path (`ensure_open` ->
  `open_index_blocking` -> `open_local(..., has_db_create_or_open(flags))`).
- I did not probe FLUSH cancellation (C2) at runtime, or nested same-key pins under a fence (C4).
- No Linux, no VM, no full matrix. The strata catalog's own correctness is taken from strata's tests and reviews;
  I read it only where Xapiand depends on it (pin lifetime, shutdown resumption, cancellation).
