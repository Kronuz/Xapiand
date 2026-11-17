# document-gate: journal

State: waiting on jobs 261116-211612-1ad7b7 261116-211637-83ad1f
Quest: .campaign/xapiand/quests/document-gate.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

A1 build, design first steps 1-3 (lead's launch 2026-11-16), at strata:efe25bcd (pushed):
repin, command codec (D3) with fixtures and fuzz target, logged mode (position with the append,
open record, sealing through a shim, watcher V, flush rule, replay). Review nits R1-R3, R5 folded
into the doc; R4 is Q5.

## Next

1. Read jobs: build-test 261116-211612-1ad7b7 (base T2/T4/T9/T11/T12/served, fuzz_command_replay,
   whole suite after the repin); TSan 261116-211637-83ad1f.
2. Nit for the next commit: each case prints "ok" even after a CHECK failed; print it only when
   the case added no failure (left out now so the queued build-test still counts).
3. Fix whatever they show; then steps 4-5 in the next launch (checkpoint, recovery's cross-checks,
   join/leave; kv_strata on a host).

## Blocked on

- Owner (design Q1-Q5): per-core logs on macOS, the wal1 label, read-your-writes under
  ack=none, a tolerant open after log damage, the 64 MiB command limit (R4). Defaults in the doc.
- Journal (Detent reviews): `TurnDriver::seal`. The shim (strata `turn_seal.hh`) shows the
  contract needs four things beyond seal(group, fn): unseal by owner, a thread-safe wake for an
  open record's first transaction, the turn budget's room passed to the sealer, and a lock order
  (turn mutex, then record mutex). Doc "Built" lists them.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | build-test 261116-211612-1ad7b7 (pending); M-j 261116-211626-0daa5f done, killed by T12's CHECK at test:253 (c's position {2,1}); M-k 261116-211632-9ff248 done, killed by T4's CHECKs at test:337-338 (refresh returned, b and c visible before the barrier); TSan 261116-211637-83ad1f (pending) |
| A2-A8 | <no evidence yet> |

## Result

strata `docs/design/standalone-turn-log.md` ("Built"); code in `include/strata/core/command.hh`,
`turn_seal.hh`, `index.hh` (logged mode), `test/standalone_turn_log_test.cc` (T2, T4, T9, T11,
T12, served), `fuzz/fuzz_command.cc`, fixtures `test/fixtures/command/`. Mutant commits on local
strata branches `mutant/document-gate/m-j` (9ff37a0a) and `m-k` (1f827405).

## Rulings I made

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
