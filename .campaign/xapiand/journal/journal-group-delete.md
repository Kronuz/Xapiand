# journal-group-delete: journal

State: working
Quest: .campaign/xapiand/quests/journal-group-delete.md (the lead's; never edit it)

## Doing

Round 1 (implementation) in the journal worktree
`/Users/kronuz/code/Kronuz/journal-wt/xapiand/journal-group-delete`, branch
quest/xapiand/journal-group-delete off journal:a3eecb5: `Host::delete_group`, the deleted table
state, the create's floor, `test/host_delete_test.cc` (6 cases), `docs/mechanisms/host-delete.md`,
CHANGELOG. Local Debug build of the test first, then jobs.

## Next

1. Build and run host_delete_test locally (Debug); fix.
2. Mutant commits under refs/scratch/journal-group-delete/ (A2: artifacts before the record; A1:
   no floor at create); jobs for A1-A3, A7, suite; push; message Detent's lead the shape.

## Blocked on

Nothing yet. File set: see Rulings (journal.h).

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | <no evidence yet> |
| A2 | <no evidence yet> |
| A3 | <no evidence yet> |
| A4 | <no evidence yet> |
| A5 | `docs/mechanisms/host-delete.md`, CHANGELOG.md 2026-11-22 |
| A6 | CI |
| A7 | <no evidence yet> |

## Result

<pending>

## Rulings I made

- R1 The old records stay in the shared turn log until retention reclaims them, so a delete cannot
  forget the id: a deleted entry (`GroupState::deleted`, state 4 in RJGT0001) keeps the
  incarnation and `base` as a floor, and `known()` passes `base` whenever it is nonzero (it was
  passed only with manifest bytes; before this the two coincided). Without it a reopen resurrects
  the old group from its last checkpoint record.
- R2 A new incarnation after a delete must not reuse the old sequences (recovery would see two
  records per sequence): `create_group` keeps `base`, and `Journal::create_shared` starts at
  `TurnDriver::retired(group)` (0 for an id never held, so unchanged elsewhere). This is one line
  in `include/journal/journal.h`, outside the file set as written: the set names `src/host*` and
  `src/turn_log*`, which do not exist in this header-only library. Lead: widen the set to
  include/journal/journal.h (and turn_log.h if meant), or say how else to start the create past
  the floor.
- R3 "Outstanding pin" read as a live hold (`holding(group)`), per the quest's decision text;
  refused with `std::system_error(EWOULDBLOCK)`, the code `hold()` gives a busy group. Not closed:
  `std::logic_error`, as the other table transitions refuse a wrong state.
- R4 A failed delete commit restores the closed entry in memory and is not "in doubt": no
  artifact goes before the record is durable, and a later commit (memory wins) or the next open
  makes either state whole.
- R5 New state value rather than a magic bump: an older binary rejects it either way (the shard
  reads as damaged); formats are open until release, and lost-bound extended the state byte the
  same way.
