# journal-lost-bound: journal

State: waiting on jobs 261121-103021-5d2ded 261121-103024-e1fed8 261121-103026-c450ec
Quest: .campaign/xapiand/quests/journal-lost-bound.md (the lead's; never edit it)

## Doing

Code pushed at journal:1e3505a (quest/xapiand/journal-lost-bound); jobs queued; CI runs on the push.

## Next

1. Read the three jobs and CI; paste `campaign brief`'s table under Evidence.
2. Lead: widen the file set (Blocked on), then the durability review; Detent's lead reviews the diff.

## Blocked on

Lead: the file set lacks four files the mechanism needs; each edit is committed and small:

1. `include/journal/turn_driver.h`: `TurnDriver::keep(group)` (6 lines). The driver owns the
   recovered records `Host::status` and `records()` read; only it can move a kept group's records
   in and take it out of `lost`.
2. `include/journal/journal.h`: a Store's recover loses a bounded group (`Host::lose`) and throws
   `Corruption` (3 lines). Without it a bounded closed-then-reopened group with no records after
   its bound takes the table-record path and restores the old base: a silent loss.
3. `CMakeLists.txt`: registers `lost_bound` in the test list and faultfs tests (2 words).
4. `test/placement_test.cc`: its damage cases asserted today's rule (victims `lost`); they now
   check each victim bounded at its bound with exactly its records after it, through every remap
   crash point, re-partition and retention.

Not in the set and not touched: `src/host*` does not exist (host.h is header-only).

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261121-103021-5d2ded queued (`lost_bound_test bounds`); 261121-103024-e1fed8 queued, `--fails-without` at the mutant journal:1d773a5 (refs/scratch/journal-lost-bound/named-lost: every named group lost) |
| A2 | 261121-103026-c450ec queued: `lost_bound_test crash second retention`, turn_log_test (double damage), turn_log_power_test, placement damage cases and all 8 remap shards |
| A3 | `docs/mechanisms/lost-bound.md` at journal:2a0ff36 |

Local runs before submitting (Debug, under quiet light): lost_bound 111 checks 0 failures;
turn_log 64/0; turn_log_power 40,913/0; journal 194,428/0; placement cases 0 failures and remap
shards 0 and 1 0 failures (400 groups, 0 lost: victims bounded).

## Result

journal:2a0ff36 and journal:1e3505a.

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
  to a second damage above it (`lost_bound_test second` checks both). Keeping the lower of two
  bounds would let a reader that covers only the lower one replay past records the higher damage
  took. Flag if the criterion meant something else.
- A named group with a kept checkpoint after the bound is not lost at all: the checkpoint covers
  every record before it (`stage` refuses one whose `covers` is below its last record).
- Bounded groups stay in `TurnRecovery::lost` until a caller that stores the bound keeps them, so
  a bare `TurnLog` (no table) loses them as before and every existing TurnLog oracle holds; only
  the Host, which stores the bound durably, keeps them.
- Shard format: a flag bit in the state byte, not a new field for every entry (no magic bump;
  pre-release, no migration).
