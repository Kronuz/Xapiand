# journal-group-delete: journal

State: waiting on jobs 261122-215035-c7dbb7 261122-215039-399df3 261122-215043-dca797 261122-215047-9ec120 261122-215050-32f31b 261122-215053-70b4bc 261122-215058-e0f727 261122-215102-3473fb 261122-215107-13499e 261122-215112-7da67f
Quest: .campaign/xapiand/quests/journal-group-delete.md (the lead's; never edit it)

## Doing

Round 1 done at journal:d89528f (pushed; CI runs on the push): `Host::delete_group`, the
`deleted` table state, the create's floor (one line in journal.h, see R2), `test/host_delete_test.cc`
(6 cases, one CTest entry each), `docs/mechanisms/host-delete.md`, CHANGELOG 2026-11-22.
Local Debug run at d89528f, every case 0 failures: reopen_empty 17 checks, stays_deleted 6,
refused 13, loop_thread 15, crash 290 (both outcomes), crash_finish 1,766. Not evidence; jobs are.

Mutant commits (local refs refs/scratch/journal-group-delete/NAME, each one hunk off d89528f):
artifacts-first journal:6fe803c (A2, the lead's: artifacts removed before the deleted record),
open-skips-finish journal:c798eb5 (A2), no-floor journal:90bd31c (A1), commit-drops-deleted
journal:4e15555 (A1), create-from-one journal:55dce72 (A1, drops the journal.h line). A local check
of the mutants was stopped before it reported (lead's request); the jobs below replace it.

Shape sent to Detent's lead via the lead: msg-245.

## Next

1. Read the jobs: A1 c7dbb7, A2 399df3, A3 dca797, A7 9ec120, suite (A4) 7da67f; fails-without
   32f31b (artifacts-first, crash), 70b4bc (open-skips-finish, crash_finish), e0f727 (no-floor,
   reopen_empty), 3473fb (commit-drops-deleted, stays_deleted), 13499e (create-from-one,
   reopen_empty). Each mutant log must show a complete build, then the named case failing.
2. CI at d89528f (A4 ASan+UBSan and TSan, A6).
3. Then State "ready for review" (durability lens).

## Blocked on

Lead: include/journal/journal.h is outside the file set (R2); widen it or rule otherwise. The suite check (`cmake -B build ...`) lacks -G Ninja; it is the lead's.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | c7dbb7 (reopen_empty, stays_deleted); fails-without e0f727, 3473fb, 13499e |
| A2 | 399df3 (crash, crash_finish); fails-without 32f31b (artifacts-first), 70b4bc |
| A3 | dca797 (refused) |
| A4 | suite 7da67f; CI asan/tsan at d89528f |
| A5 | `docs/mechanisms/host-delete.md`, CHANGELOG.md 2026-11-22 |
| A6 | CI at d89528f |
| A7 | 9ec120 (loop_thread, owner check compiled in) |

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
