# x7-loop-fix-c review: design

Commit: 45815797d4130744e1810ff14b7d1d3a57e0287c
Verdict: CHANGES
Date: 2026-11-12

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

Scope: the section "Design: fix (c), RESTORE's writes off the loop, one shard per child" of
`docs/perf/x7-loop-stalls.md` at this commit, against the quest file, phase7c.md D4/D12, master's
`DocIndexer::index` (`src/database/handler.cc:2342-2618`), `commit_blocking` (`src/database/shard.cc:512-541`),
`engine::counted` (`src/engine/host_asio.h:111-229`) and the two x7-loop-stalls reviews. The shape is right:
the write moves whole into a chore while a task holds the turn, which keeps D12 and adds no pool. Both
blocking findings are text fixes to the design and its test list.

## Evidence I relied on

- Job 261112-012857-a77bf8 (A3 at Xapiand:0072ba93e): its `bulk/pb-{3,6}-cur.sample.txt`, re-parsed below.
- No job establishes A2-A4 yet; this review rules only A1.

## What I checked myself

- The profile numbers the T12 estimate rests on, recomputed from the job's raw `sample(1)` output with
  `.scratch/review-x7-loop-fix-c-design/shares.py` (a local parse of the job's files, no build, no timing;
  rerun: `python3 -I shares.py <job>/a1/out/bulk/pb-3-cur.sample.txt <job>/a1/out/bulk/pb-6-cur.sample.txt`).
  The RESTORE loop: 2,807 and 2,882 busy samples; `finalise_write` 46.0% and 45.7%; `apply_settling` 9.3% and
  8.6%; zstd compress 29.2% and 29.4%; placement (`written_key`) 0.2% and 0.4%; the whole turn
  (`Shard::replace*`) 69.3% and 70.6%. The note's figures reproduce.
- Q1, turnless users of the engine's chores. Every `chores_` use in `src/engine/strata/shard.cc` (apply_t,
  commit, transactions, set_metadata, reopen, refresh, reclaim, close, check) is reached from `src/database`
  only under a turn (`fresh`, `pool.cc:362-371`; `written_key`/`written_count`/`check`, `shard.cc:1343-1356`,
  `:1627`; the writes, `shard.cc:1479-1613`) or with no pin (`open_blocking`, `commit_blocking`). `reclaim`,
  `begin_close` and `finish_close` have no caller in `src/`. Reads (`get`, `scan`, `find_key`, `metadata`) never
  read `chores_`. The swap is sound under the turn.
- Q1, state touched off the loop by `do_replace`/`do_replace_term` (`shard.cc:736-870`): `applied_`,
  `modified`, `stale_` (`note_failure`) and `core.dirty` are atomics; `core.pending` and the engine's index are
  turn-owned and change hands through the turn and the chore's completion; the blob push already runs on
  workers; RESTORE writes no WAL (`DB_DISABLE_WAL`, `search_views.cc:1682`); census is mutex-guarded;
  `unserialise_term_id` is pure. Nothing loop-bound.
- Q1, cancel and throw mid-chore. `IndexChores::run` takes no token (`pool.h:93-121`, `chores.h:42-47`): the
  awaiter resumes only after `fn` ends, or with `task::cancelled` when `cancel_all` drops it before it starts.
  So the child never releases the turn under a running chore, and a guard on the worker restores the engine's
  chores before the completion is posted.
- Q2, nested `engine::counted`. A child's `counted` ends on the loop's scheduler, and task's final awaiter
  (task bdafa5f `task.hh:517-523`) posts the continuation to the scheduler recorded when it was awaited, the
  request's `Counted`, so the parent keeps its counters after the join. Each child has its own
  `TaskCounters` (`host_asio.h:126`), so `take_turn`'s assert (`pool.cc:354`) checks each child.
- Q2, one turn per task and deadlock. A child takes one turn at a time and waits only on a chore; chores never
  wait on a turn (`commit_blocking` try-locks, and only on an unpinned entry, `shard.cc:524-525`); the parent's
  `fetch`, `accept`, placement and the serial fallback run only when no child is alive, each `lock_shard` scoped
  to one shard (`least_used_shard`, `find_docid_term`). No hold-and-wait cycle exists, with a concurrent writer
  on another shard or another RESTORE.
- Q3, placement. Per shard, the state after a write depends only on that shard's sequence of writes
  (`next_docid`, `new_did`, versions in `replace_by_key`, `strata/shard.cc:925-982`). With no erase in a range,
  the range-start lookup plus `numeric_docid`/`key_placement_hash` names the shard the serial path would, for
  every object, including a numeric `_id` written after a `NEW_NUMERIC_KEY` object assigned it (a numeric id's
  shard is `(id - 1) % n` both ways). With input order kept per shard, ids, versions and records equal a serial
  restore's. The one input that breaks it is B1's.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| B1 | yes | defect | The `must_exist` re-place breaks the design's own rule "per id, writes apply in input order". Input: a RESTORE into a live index whose stream holds objects i < j for numeric `_id` 7, found at range start in shard 1 at docid 2. A concurrent DELETE of 7 lands before i's apply, so i misses and is deferred to the serial pass. A concurrent PUT of 7 lands before j's apply and takes docid 2 again (`new_did`, free after the erase, `shard.cc:838-840`, `strata/shard.cc:943-945`). j's `do_replace(2, must_exist)` then succeeds in the child, and after the join the serial pass applies i over it. The stored document ends as i, an earlier object, which no serial interleaving gives. The design leaves this path "argued here, left to the review" and untested. | `x7-loop-stalls.md` "Placement" and "Ordering"; `handler.cc:1966-1982` | State that a miss defers that object and every later object of its term in the range, applied serially in input order after the join (or the child stops its sub-run at the miss). Add a deterministic test through a seam between placement and apply (an erase injected there), with a mutant that defers only the missing object. |
| B2 | yes | gap | The planned mutants can pass vacuously. A range is what the prepare chores pull in `PREPARE_SLICE` of wall time (`handler.cc:2294`, `:2498`). The test node has 4 processors, so prepare width is `max(1, threads() - 1 - 5) = 1`. On a Debug or TSan build, a range can then hold one to a few objects. The reverse-sub-run mutant fails only when two objects of one id share a sub-run, and the no-join-before-`NEW_NUMERIC_KEY` mutant only when a range holds more than one such object. Only "Shards in parallel" asserts that the case it tests happened. | `x7-loop-stalls.md` "Tests" | Make each test assert its case happened, with `DocIndexer` reporting the largest range and the repeated ids within a sub-run (as `prepared()` does), or give the tests a seam that sets the range size (objects per chore, not wall time). |
| N1 | no | gap | A turn is held while its chore waits in the pool's queue, not only for its run of about 1 ms. With a saturated pool, or with more shards than non-reserve workers (children are not capped), every shard's turn can be held by a chore that has no worker. A read-your-writes refresh needs the turn (`fresh`), so the query class's reserve worker does not help it. A2's "can wait that long" understates this for the same-index run. | `x7-loop-stalls.md` "Children" and "What each row should gain"; `pool.cc:362-371` | Cap the children holding a turn at the apply width (a child takes its turn when an apply slot is free), or report the turn hold and wait in A2's same-index run. |
| N2 | no | nit | "commit_blocking's shape" should name the chores argument. `do_replace*` push blobs through the `chores_` argument (`shard.cc:743`, `:805`), which callers set to the endpoint's own `IndexChores` today (`shard.cc:1537`, `:1557`). Inside the apply chore, that would block a worker in `sync_wait` on another chore of the same pool. | `shard.cc:539`, `:1537`, `:1557` | Say that the chore passes `engine::inline_chores()` to `do_replace*`, as `commit_blocking` does to `do_commit`. |
| N3 | no | nit | The per-range lookup must keep `find_docid_term`'s semantics: shards in order, first hit wins, per-shard retry and reopen, and `not_available` failing only the objects found in no shard. Otherwise one unavailable shard changes many objects' results. | `handler.cc` `find_docid_term` | One sentence in "Placement", and a case in the model test if cheap. |
| N4 | no | gap | Failures widen. A child whose pin or turn fails (busy, closed) fails its sub-run's remaining objects, where serial gives each object its own turn wait. Inside a slice, an error that marks the endpoint stale (`note_failure`) leaves later objects to write to the stale index, where serial reopens on release (`shard.cc:1310-1312`). | `x7-loop-stalls.md` "Failure and cancel" | Owner's call. Option 1: keep it. Option 2: end the slice at a stale-marking error, and on busy fail the slice's first object and retry the rest. |
| N5 | no | gap | A4 asks for a test per new handoff, and the chores swap is one. No planned test fails if the guard skips restoring the engine's chores after a write that throws. | `x7-loop-stalls.md` "Tests" | A unit test: after an apply chore whose write throws, the engine's chores are the endpoint's again (a debug accessor). Mutant: no restore on the throw path. |
| N6 | no | gap | The T12 estimate is grounded in the profile for the CPU per document and the loop's shares, which reproduce. But "at pre-7c's 5.9 busy cores" is assumed, not derived. By the same samples, the loop keeps about 30% of its RESTORE time, not 45-46%, since `Shard::replace*` is 69-71%. The bounds after the fix are elsewhere: per-shard writes of about 0.1 ms per document over 5 shards, the prepare width, and the round barrier (`when_all` of the apply and the next prepare). | `shares.py` on job 261112-012857-a77bf8 | Name the critical path after the fix in one line. A3 decides either way. |
| N7 | no | nit | "With no reactor ... the sub-runs run one after another, inline, as today": "as today" should mean today's `replace_document_term` path, not the apply chore run inline. An inline chore would nest `engine::sync_wait` on the CLI's thread. | `x7-loop-stalls.md` "Children" | Say which. |

## Not run

- No probe job. The disputes were settled by reading the code. B1 is an interleaving argument from the cited
  lines; its test is the fix's.
- No build, test, TSan or timing. The design is not built. The x7-loop-stalls correctness review's
  `restore_model.py` (`.scratch/review-x7-loop-stalls-correctness/`) was not rerun.
- The catalog's commit chore racing a re-pin between a child's slices (`commit_blocking`'s try-lock assert) is
  pre-existing and unchanged by this design. Not examined further.
- E4 and the T4/T5 analysis of the same section: not part of the concurrency questions; not reviewed.
