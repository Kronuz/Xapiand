# document-gate: journal

State: waiting on jobs 261117-030334-5d9e3a 261117-030338-64ed0e 261117-030347-7f586d 261117-030357-c294a1 261117-030405-14df88 261117-030412-4cd813 261117-030420-64cdd2
Quest: .campaign/xapiand/quests/document-gate.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

A1, step 6 (the real seal) at strata:395644a7 (pushed; docs strata:706cdaf5): journal repinned to
555b69f; the driver seals through TurnDriver::seal/unseal/wake; turn_sealer stays as the
synchronous host loop (turn mutex = loop context, inline or served turns, watchers after a barrier);
checkpoint records carry `covers`. Whole test passes locally (8 s); kv_strata wal and sync rows ran
once as a strict smoke (beside 14 light holds, numbers not usable): fillrandom wal 375k ops/s;
fillrandom sync batch 10, 249.8 ops/s (60 s cap); ycsb_a sync batch 1, 86.9 ops/s, p50 20.6 ms.

## Next

1. Read jobs at strata:395644a7: build-test 261117-030334-5d9e3a, TSan 261117-030338-64ed0e,
   seal mutants M-j 261117-030347-7f586d (T12), M-k 261117-030357-c294a1 (T4), M-a
   261117-030405-14df88 (T1), M-c 261117-030412-4cd813 (T4), M-h 261117-030420-64cdd2 (T7b).
   Build-test 261117-024952-c870f5 (40f89d0f) was still running; superseded by the new one.
2. For a fresh contributor: D7 join/leave (turn_group attribute in the manifest log, host
   create/close/reopen_group, a checkpoint at close), T6; D6.3's closed/none statuses through the
   Host.
3. Step 5 rest: M1 faultfs counter assertion; M2/M3 sync1 and wal1 rows (exclusive; the smoke
   suggests about two device flushes per barrier on macOS at batch 10: check in M2); M4 floor; M5
   checkpoint stall; docs/perf/standalone-turn-log.md.
4. Stale design text: D3's "until it lands" and Limits' "Until TurnDriver::seal lands" bullets.

## Blocked on

- Owner (design Q1, Q2): per-core logs on macOS, the wal1 label. Q3-Q5 ruled option 1 (D103,
  Xapiand:1935c388d), closed in the doc at strata:8b91d151; quest turn-log-parity follows.
- Journal (Detent reviews): `TurnDriver::seal`. The shim (strata `turn_seal.hh`) shows the
  contract needs four things beyond seal(group, fn): unseal by owner, a thread-safe wake for an
  open record's first transaction, the turn budget's room passed to the sealer, and a lock order
  (turn mutex, then record mutex). Doc "Built" lists them.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 (step 6, real seal, strata:395644a7) | pending: build-test 261117-030334-5d9e3a; TSan 261117-030338-64ed0e; fails-without M-j 261117-030347-7f586d, M-k 261117-030357-c294a1, M-a 261117-030405-14df88, M-c 261117-030412-4cd813, M-h 261117-030420-64cdd2 |
| A1 (steps 4-5, shim, strata:40f89d0f) | TSan 261117-025002-c70c5f passed; T1 200-seed sweep 261117-025006-d345af passed; all 12 mutants killed: M-a 261117-025010-4c8220, M-b 261117-025013-a6cec4, M-c 261117-025017-0b218d, M-d 261117-025020-93f7da, M-e 261117-025024-24bfef, M-f 261117-025028-47bf98, M-h 261117-025032-8fc28b, M-i 261117-025035-d9bfa2, M-l 261117-025039-d9b68d, M-m 261117-025043-a4a6fc, M-n 261117-025046-ec1d6e, M-o 261117-025050-ee8915 (exit 1, or 134 where the mutant aborts the run) |
| A1 | steps 1-3: build-test 261117-020047-53b736 passed; TSan 261116-211637-83ad1f passed; M-j 261116-211626-0daa5f, M-k 261116-211632-9ff248 killed. Steps 4-5 at strata:40f89d0f: build-test 261117-024952-c870f5 (running); TSan 261117-025002-c70c5f passed (served, T4, T12, T3, T8); killed so far: M-a (exit 1), M-b (exit 134, abort), M-c, M-d (exit 1); the rest running or queued; T1 200-seed sweep 261117-025006-d345af; fails-without: M-a 261117-025010-4c8220 (T1), M-b 261117-025013-a6cec4 (T1), M-c 261117-025017-0b218d (T4), M-d 261117-025020-93f7da (T5), M-e 261117-025024-24bfef (T7), M-f 261117-025028-47bf98 (T10), M-h 261117-025032-8fc28b (T7b), M-i 261117-025035-d9bfa2 (T7b), M-l 261117-025039-d9b68d (T8), M-m 261117-025043-a4a6fc (T3), M-n 261117-025046-ec1d6e (T5), M-o 261117-025050-ee8915 (T5) |
| A2-A8 | <no evidence yet> |

## Result

strata `docs/design/standalone-turn-log.md` ("Built"); code in `include/strata/core/command.hh`,
`turn_seal.hh`, `index.hh` (logged mode), `test/standalone_turn_log_test.cc` (T2, T4, T9, T11,
T12, served; now T1, t1r, T3, T5, T7, T7b, T8, T10 too), `fuzz/fuzz_command.cc`, fixtures
`test/fixtures/command/`, `bench/kv/kv_strata.cc` (host). Mutant commits on local strata branches
`mutant/document-gate/m-*` (m-j 9ff37a0a, m-k 1f827405; m-a..m-o on 8855d678, applied by
`.scratch/mutate.py` in the strata worktree).

## Rulings I made

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
