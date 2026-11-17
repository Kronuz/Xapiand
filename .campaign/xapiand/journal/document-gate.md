# document-gate: journal

State: waiting on jobs 261117-053808-828075 261117-053809-785b59 261117-053813-188303 261117-053816-89350c 261117-053837-28f268 261117-053837-a2fba7 261117-053837-309a68 261117-053855-33a5e4 261117-053858-25929d 261117-053901-da726f 261117-053905-5fd8ef 261117-053908-02532a 261117-053911-ae5217 261117-053915-f65b74 261117-034940-b2b9c3
Quest: .campaign/xapiand/quests/document-gate.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

The sync batch fix at strata:5e7d1178 (pushed). kv_strata's engine moved to
`bench/kv/kv_strata.hh`; a sync phase's writes join one transaction applied at durable() (RocksDB's
WriteBatch is one synced write), split only at the 1 MiB command limit; on_phase turns batching
on for sync phases only (the load runs at none). `test/kv_strata_batch_test.cc` runs the engine
over faultfs (host and index on separate filesystems) with its served loop: 32 batches of 16
writes (duplicates and an erase inside) give 32 records, 32 barriers, 32 writes, and a 1.6 MiB
batch splits into two records. Mutant M-r (strata:21df4764, branch mutant/document-gate/m-r):
batching off, each write its own transaction. run.py's `aa` takes a set; `syncaa` is sync1's
fillrandom b100, ycsb_a b1, fillrandom b1. Not built locally (light slots were held by a pending
exclusive); the jobs' build is the first.

## Next

1. Jobs at strata:5e7d1178: build-test 261117-053808-828075; batch test 261117-053809-785b59;
   M-r fails-without 261117-053813-188303; batch test TSan 261117-053816-89350c; seal TSan
   (served, T4, T12, T3, T8) 261117-053837-28f268; t6w TSan 261117-053837-a2fba7; T6+t6w+T5
   261117-053837-309a68. M-d at its mutant (f55 base) 261117-052940-986f82.
   Rows, exclusive, after the batch test: sync1 a/b/c 261117-053855-33a5e4,
   261117-053858-25929d, 261117-053901-da726f; wal1 t3 261117-053905-5fd8ef; A/A syncaa x/y
   261117-053908-02532a, 261117-053911-ae5217; M2 strict 261117-053915-f65b74. N-log (old code,
   batch 1 rows, unaffected) 261117-034940-b2b9c3 running.
2. Write docs/perf/standalone-turn-log.md from those rows: four-axis table, the honest gap (D103,
   D104), the ack=none window (Q3: one barrier plus one write, ~21 ms on macOS). Prior rows at
   9d7c7710 (before the fix) as the before column: sync1 261117-033324-78da56 (fillrandom b100
   1.98x slower, overwrite b100 1.99x, b10 1.53x, b1 rows tie), wal1 261117-033320-c0fc7f
   (5.54x/6.80x/4.66x faster), M4 floor 261117-034943-316356 (48.4/s, p50 20.2 ms).
3. Then A2/A8's gate report (docs/gates/document-profile.md) and the Evidence table.

## Blocked on

- (none from the owner: Q1/Q2 ruled D104 by the lead, Q3-Q5 D103.)
- Journal (Detent reviews): `TurnDriver::seal`. The shim (strata `turn_seal.hh`) shows the
  contract needs four things beyond seal(group, fn): unseal by owner, a thread-safe wake for an
  open record's first transaction, the turn budget's room passed to the sealer, and a lock order
  (turn mutex, then record mutex). Doc "Built" lists them.

## Messages

- msg-1 (lead, journal-host-guards): repinned journal to cc95ba6 (landed). turn_sealer mirrors
  durable(): a wait on a group closed here, or at or below what its closed life retired, throws
  journal::group_closed; await_durable catches it by type and throws index_closed (header doc:
  after a leave the transaction is durable; after a close that could not leave, recovery tells).
  The restaged table record follows max(base, retired). Tests: t6w (a waiter blocked across
  close, then an old-life wait after reopen on the same host, then recovery over the gap at
  base + 2); build-test, TSan and T6 rerun as jobs (ids under Next).

## Evidence

| Criterion | Evidence |
|---|---|
| A1 (step 6, real seal, strata:395644a7) | pending: build-test 261117-030334-5d9e3a; TSan 261117-030338-64ed0e; fails-without M-j 261117-030347-7f586d, M-k 261117-030357-c294a1, M-a 261117-030405-14df88, M-c 261117-030412-4cd813, M-h 261117-030420-64cdd2 |
| A1 (steps 4-5, shim, strata:40f89d0f) | TSan 261117-025002-c70c5f passed; T1 200-seed sweep 261117-025006-d345af passed; all 12 mutants killed: M-a 261117-025010-4c8220, M-b 261117-025013-a6cec4, M-c 261117-025017-0b218d, M-d 261117-025020-93f7da, M-e 261117-025024-24bfef, M-f 261117-025028-47bf98, M-h 261117-025032-8fc28b, M-i 261117-025035-d9bfa2, M-l 261117-025039-d9b68d, M-m 261117-025043-a4a6fc, M-n 261117-025046-ec1d6e, M-o 261117-025050-ee8915 (exit 1, or 134 where the mutant aborts the run) |
| A1 | steps 1-3: build-test 261117-020047-53b736 passed; TSan 261116-211637-83ad1f passed; M-j 261116-211626-0daa5f, M-k 261116-211632-9ff248 killed. Steps 4-5 at strata:40f89d0f: build-test 261117-024952-c870f5 (running); TSan 261117-025002-c70c5f passed (served, T4, T12, T3, T8); killed so far: M-a (exit 1), M-b (exit 134, abort), M-c, M-d (exit 1); the rest running or queued; T1 200-seed sweep 261117-025006-d345af; fails-without: M-a 261117-025010-4c8220 (T1), M-b 261117-025013-a6cec4 (T1), M-c 261117-025017-0b218d (T4), M-d 261117-025020-93f7da (T5), M-e 261117-025024-24bfef (T7), M-f 261117-025028-47bf98 (T10), M-h 261117-025032-8fc28b (T7b), M-i 261117-025035-d9bfa2 (T7b), M-l 261117-025039-d9b68d (T8), M-m 261117-025043-a4a6fc (T3), M-n 261117-025046-ec1d6e (T5), M-o 261117-025050-ee8915 (T5) |
| A1 (D7, M1, strata:9d7c7710) | pending: build-test 261117-033227-678742; TSan 261117-033250-38d2df; T1 sweep 261117-033250-f42f82; fails-without M-p 261117-033229-e6ebfc, M-q 261117-033233-7e4d9b, M-o 261117-033236-b7fbb6, M-d 261117-033240-5df553, M-n 261117-033244-6cd288, M-b 261117-033248-e735a3 |
| A1 (step 6 seal, strata:395644a7) | TSan 261117-030338-64ed0e passed; killed: M-j 261117-030347-7f586d, M-k 261117-030357-c294a1, M-a 261117-030405-14df88, M-c 261117-030412-4cd813, M-h 261117-030420-64cdd2 |
| A2-A8 | <no evidence yet> |

## Result

strata `docs/design/standalone-turn-log.md` ("Built"); code in `include/strata/core/command.hh`,
`turn_seal.hh`, `index.hh` (logged mode), `test/standalone_turn_log_test.cc` (T2, T4, T9, T11,
T12, served; now T1, t1r, T3, T5, T7, T7b, T8, T10 too), `fuzz/fuzz_command.cc`, fixtures
`test/fixtures/command/`, `bench/kv/kv_strata.cc` (host). Mutant commits on local strata branches
`mutant/document-gate/m-*` (m-j 9ff37a0a, m-k 1f827405; m-a..m-o on 8855d678, applied by
`.scratch/mutate.py` in the strata worktree).

## Rulings I made

- Sync batch (2026-11-17):
  - A sync batch is one transaction: the fair mapping of RocksDB's WriteBatch (both make a batch
    visible at once and atomically). Gets inside a batch miss its pending writes, as RocksDB's do.
  - The engine moved to a header so a test runs the bench's own code; the IO is injected
    (PosixIO by default, faultfs in the test). The batch test counts the host filesystem's syncs.
  - The many-index row's durable() now applies and awaits every shard (it awaited shard 0 only).
  - A/A on the sync rows as two one-trial jobs, to fit the 10-minute exclusive bound.

- D7, join and leave (2026-11-17):
  - A closed group reopens at open, not at the first write (D6.3 said the first write): journal's
    Store does the same, and an apply never does a durable table update. D6.3 updated.
  - The join's first record waits for the attribute's edit: create joins at its first commit, and
    a logged create_deferred refuses apply (logic_error) until that commit.
  - An index with turn_group opens only with its host (log_mismatch), so no build writes past the
    log; read-only tools decode the head as before. Demotion to unlogged is not built.
  - close() leaves; a failed leave throws before the teardown (the group stays stored, replayed at
    open); the destructor does not leave. kv_strata still destroys without close (crash-like reopen).
  - turn_sealer takes the Host, its loop and identity (Host has no identity accessor; journal
    unchanged). Table calls run under the turn mutex, since close_group calls the driver's forget.
  - Records empty but the log holds the group past the head (the host released it earlier in this
    process): log_lost, "reopen the host", as journal's Store refuses it.
  - The table path refuses a head past the table's record (log_mismatch); invariants make it
    impossible.
  - T7 and T10 measured file bytes through an unlogged open; now through a logged open with nothing
    to replay (same accounting).
  - The A1 mutant set: M-b, M-d, M-n, M-o rerun because their code moved; M-o needed a new T5 case
    (a same-identity host elsewhere), since the attribute check now refuses the old case first.
    Mutants come from `.scratch/mutate.py` via `.scratch/mkmutant.sh` (strata worktree).
  - Row jobs split to fit the 10-minute exclusive bound: a --long job is deferred and proves
    nothing; sync1 runs as three one-trial jobs.

- Step 6, the real seal (2026-11-17):
  - turn_sealer kept as the blocking host loop rather than journal::run: strata's owner thread
    is synchronous and needs inline turns, a serving thread and per-group watchers.
  - Seal mutants rerun at the new commit: M-j, M-k, M-a, M-c, M-h; the others don't touch it.
- Steps 4-5 (2026-11-17):
  - Replay runs inside open() (eager, D6.6); the public replay() is gone. The checkpoint record
    goes through the sealer as a closed record, so the loop alone calls the driver.
  - Checkpoint triggers built: apply's inline flush and checkpoint_log_bytes (default 64 MiB; off
    under defer_io, whose host owns flushes). close() does not checkpoint until D7's leave.
  - M-g replaced by an invariant check (for the reviewer to judge): D5's lease span is not a
    separate mechanism in strata. commit() holds one writer_scope from the log wait to the
    checkpoint record's staging; apply() needs the same lease on the same owner, so a mutant
    "releasing the lease after step 1" has no code to change without adding a second scope that
    nothing calls between. What would break if it did, a batch between X and the record, is
    refused twice: log_state::checkpoint throws unless no record is open or queued and X is the
    record before it, and journal 555b69f's log refuses a checkpoint whose `covers` is behind a
    record (T11 checks the latter). No stepping executor in T1.
  - Extra mutants for claims this step adds: M-m (fail-stop fence), M-n (head older than the
    checkpoint), M-o (checkpoint UUID).
  - M-a is "the checkpoint record made durable before the edit": staged alone it would not reach
    the log before commit returns in a blocking test.
  - A fallback open (`to`) commits its older head before attach_log refuses it with log_lost; the
    next open refuses again (loud). Left as is.
  - kv_strata: logged rows put indexes in <dir>/<i> and the host in <dir>/host (disk counted);
    1 MiB command limit, 4 MiB segments (the shipped default would preallocate 129 MiB).
  - T1 default 8 seeds per crash point (3 s whole test); the job runs 200.

- Edited cmake/check_core_includes.cmake, outside the file set, at the lead's request: journal/turn_driver.h joins the allowlist (journal is already a core library).

- Build launch (steps 1-3):
  - The repin also moves completion to 7731315 and task to 95798a0 (journal 7c08540's pins) and
    sets COMPLETION_TASK ON: journal's turn driver needs completion::task, and strata's
    declarations win FetchContent.
  - Transaction types moved from index.hh to command.hh, so the codec and the index share them.
  - Codec: role and postings bytes decoded raw (stage 1 judges them; a replicated command may be
    invalid), flags/mask/kind strict; unknown kind/version unsupported_index, else corrupt_index.
  - The append runs after publish_cover, so V never passes the cover (D7 rule 4); a throw in it
    fences (an accepted transaction the log lacks).
  - Records stay whole: the transaction that would take a record past command_bytes closes it;
    positions never move between records.
  - Views of a logged index keep their predecessor's metadata/fields/schema/last docid while the
    applied position is past the log (those are not filtered by V); flush() and apply_pending()
    (pending deletes into segment bits, also run by merge()) await the log.
  - Owner-thread reads (index.get under the lease) see applied state; D4 governs reader handles.
  - attach_log refuses a head past the log's durable sequence with log_lost (D6.4, early).
  - T2 and T12 put the log in the index's faultfs directory (no name overlap), so one cut orders
    both; faultfs gains power_on() for a process crash.
  - M-k is T4's flush wait only; apply_pending's wait has no mutant of its own (no deterministic
    publication reaches it).
  - Syntax checks ran locally under `quiet light` (fsyntax-only, older deps); builds are jobs.


- Review fold-in (revision 1), by finding:
  - F1: lease from seal to checkpoint record; V and acks published by a watcher without the lease. Tests T1, T11; mutant M-g.
  - F2: owed verdicts run their chore at replay; no checkpoint until release. Test T7b; mutants M-h, M-i.
  - F3: position taken with the append under the record mutex; sealing by the driver. Test T12; mutant M-j.
  - F4: 64 MiB command limit at admission (replicated ceiling) and a half-segment turn budget. Test T8; mutant M-l.
  - F5: every flush on a logged index awaits the log. Test T4; mutant M-k.
  - F6: T1 writes after recovery (M-b); T10 refuses at runtime (M-f).
  - N1: D7 and D9 follow the host group table and fail-stop.
  - N2: Q1 uses P2, and M3 gains an N-log row.
  - N4: parity claimed for throughput and median only.
  - N6: group ids are never reused.
  - N7: a failed turn reports `outcome_unknown`.
- No strata turn runner: journal main's `TurnDriver::run()` and `journal::run` drive turns.

- Wrote `docs/design/standalone-turn-log.md`, outside the quest's file set, because the lead
  assigned that path.
- Apply-first (log only accepted transactions), not log-first (D2).
- Visibility waits for the barrier on logged indexes (D4).
- Checkpoint waits for the log through its position (I1), accepting a 1-2 barrier writer stall
  per checkpoint until past-generation checkpoints exist.
- Kept Track C's torn-turn rule, rejecting the manifest log's zero classifier for recycled
  segments (D10).
- The doc runs about 3,800 words, over the design-doc ceiling, because the assignment needs
  its tests, jobs and questions spelled out; most of the length is tables.
