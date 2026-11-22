# journal-lost-bound: journal

State: durability accepted at journal:e4b0100 (F1 confirmed by 143537-209eff and 143537-3a98cc; fails-without 143022-821a58 at mutant 6d1502c). Lands after detent:journal-tail-second-sector; CI at the head next.
Quest: .campaign/xapiand/quests/journal-lost-bound.md (the lead's; never edit it)

## Doing

Durability review (CHANGES at journal:ebd3038), one round:

- Merge: journal main (journal:60ab864, turn-buffers) merged at journal:6cc553d. Conflict in
  turn_log.h's contiguity loop: kept the branch's `record_of`/`live_of` loop. F4: `live_of` passes
  `item.payload.size()`, and `Live` now takes every field through its constructor, so a 4-field
  `Live{...}` no longer compiles (the guard against a silent 0).
- F1 (blocking): journal:c851826. A gap item records `last`, its last block whose bytes may begin
  a lost record (bad fragment, damaged header, failed read, short record; not a block holding only
  the rest of a record the gap already covers, so a record spanning blocks costs no intact
  neighbor). `gap_damage` names every manifest from the gap's first block through `last`, and is
  `unknown` when one is missing. Test: `lost_bound_test spans` (the reviewer's layout).
  `--fails-without` at the mutant journal:6d1502c (refs/scratch/journal-lost-bound/gap-first-block:
  the head with turn_log.h's F1 hunks reverted, i.e. ebd3038's first-block rule; ebd3038 itself
  has no spans case): 261121-143022-821a58.
- F2 (pre-existing): journal:80e58b5 narrows lost-bound.md: the crash claim holds for damage a
  later kept turn acknowledges; Limits states the rule-5 cut with jobs 261121-141538-9a63b7 and
  261121-141627-fe334f.
- F3: the owner's call (keep, and land strata's pin bump only with tolerant-open; or refuse a
  bounded group's `stage` until a `resume(group, head)`); the lead posts it. Nothing changed here.
- F4: see Merge.
- F5: A2's "a second damage keeps the lower bound" reads, in the code, as the bound never lowers
  and rises to a second damage above it: the maximum is kept (`lost_bound_test second`). The
  quest's wording is the lead's.
- F6: CI runs on the push of this round.

Jobs at journal:80e58b5: cf1f10 (A1: lost_bound whole incl. spans, host_test whole); c78107 (A2
and the crash cases on faultfs: lost_bound crash/second/retention/spans, turn_log, turn_log_power,
turn_buffers power/forward, turn_bound crash, turn_seal crash, placement damage cases and 8 remap
shards); e9efc6 (TSan: lost_bound whole and host_test whole); 131df7 (suite: ctest whole);
1352b6 (the reviewer's multi_block_gap_probe, expecting exit 0). `campaign verify` queued nothing
(its gate is CI).

## Next

1. Read the six jobs and CI (pushed journal:80e58b5; CI is the gate).
2. Lead: `campaign accept journal-lost-bound durability --confirm F1=261121-143537-3a98cc`
   once the suite job passes; widen the file set (Blocked on).

## Blocked on

Nothing. Items 1-5 are in the file set (they were already); item 6, turn-log-damage.md rule 3, is
widened and edited by the lead at journal:e4b0100. The jobs at 80e58b5 were moved to e4b0100 by
`campaign job rerun` before any ran.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | At journal:ebd3038: 261121-105412-c88261 passed (`lost_bound_test bounds`), 261121-105355-e0cfd0 passed (host_test whole); `--fails-without` at the mutant journal:63494f8 (every named group lost): 261121-105415-3c8f01 (bounds), 261121-105358-5bfdac failed as required (idle_damage). At journal:1e3505a: 5d2ded passed, e1fed8 failed at the mutant journal:1d773a5 as required |
| A2 | At journal:ebd3038: 261121-105413-504c72: `lost_bound_test crash second retention`, turn_log_test (double damage), turn_log_power_test, placement damage cases and all 8 remap shards (passed at journal:1e3505a as c450ec) |
| A3 | `docs/mechanisms/lost-bound.md` at journal:2a0ff36 |

Local runs before submitting (Debug, under quiet light): lost_bound 111 checks 0 failures;
turn_log 64/0; turn_log_power 40,913/0; journal 194,428/0; placement cases 0 failures and remap
shards 0 and 1 0 failures (400 groups, 0 lost: victims bounded).

## Result

journal:2a0ff36, journal:1e3505a and journal:ebd3038 (idle_damage's expectations).

- `TurnLog::recover`: `named` carries each group's highest sequence the damage could hold. A named
  group whose latest kept checkpoint follows the bound recovers whole; one without is lost and
  bounded (`TurnRecovery::bounded`: the bound and its records after it, which must start at
  bound + 1). A kept manifest naming more than a group holds bounds it there. `unknown` damage
  still loses every group. `TurnLogGroups::bound` (the table's stored bounds) supersedes records
  through it, so later damage at or below it names nothing and the group may start at bound + 1.
  `TurnLog::keep(group, bound)` makes a bounded group live again; `TurnLog::refuse` refuses an id.
- `Host`: `GroupEntry::bound`, stored as the state byte's `bounded_state` bit plus 8 bytes (zero
  bytes for every other entry, so existing tables read unchanged); `Host::open` commits bounds
  (`mark_bounds`) before it serves, then keeps those groups; a re-partition stores them before its
  layout commit and starts each in the new log after its bound. `GroupStatus::bounded`,
  `Host::bound`, `Host::lose`. A bound clears at an open whose log kept a checkpoint after it, and
  at a close.
- Doc: journal `docs/mechanisms/lost-bound.md`; CHANGELOG 2026-11-21.

## Rulings I made

- A2's "a second damage keeps the lower bound": implemented as the bound never lowers, and rises
  to a second damage above it, so the maximum is kept (`lost_bound_test second` checks both). Keeping the lower of two
  bounds would let a reader that covers only the lower one replay past records the higher damage
  took. Flag if the criterion meant something else.
- A named group with a kept checkpoint after the bound is not lost at all: the checkpoint covers
  every record before it (`stage` refuses one whose `covers` is below its last record).
- Bounded groups stay in `TurnRecovery::lost` until a caller that stores the bound keeps them, so
  a bare `TurnLog` (no table) loses them as before and every existing TurnLog oracle holds; only
  the Host, which stores the bound durably, keeps them.
- Shard format: a flag bit in the state byte, not a new field for every entry (no magic bump;
  pre-release, no migration).
- F1's range stops at the last block that may begin a lost record, not at the block where the
  next record starts: a block holding only the continuation of a record the gap already covers
  names nothing new (each block's manifest names every record touching it), and reading it
  would bound intact records after the gap (`second`'s one-7, `bounds`' one-3/four-3).
- F4's guard is a constructor on `Live`, not a runtime check: a payload of 0 bytes is legal, a
  missing initializer is not.
