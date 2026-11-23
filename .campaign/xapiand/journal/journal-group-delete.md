# journal-group-delete: journal

State: waiting on jobs 261123-010037-440d3e 261123-010040-e5a982 261123-010043-3e6bf7 261123-010046-03aa5c 261123-010048-467c0f 261123-010051-5f60a2 261123-010054-597440 261123-010057-6b2d97 261123-010100-86e760 261123-010006-f8221d; CI run 45143330663 at journal:d8453f6
Quest: .campaign/xapiand/quests/journal-group-delete.md (the lead's; never edit it)

## Doing

Round 2 at journal:d8453f6 (pushed): CI run 45120376454 at d89528f failed on linux-x86_64-gcc only,
reopen_empty "c stays closed with its checkpoint: {c-state:}". Cause, in the test: that check's
condition read `node.status(c)` and its message called `show(node.recover(c))`; function
arguments run in no fixed order, GCC built the message first, the recover opened c, and the status
read open. The recovered state was right. Fix (test only): read c's status, then each recovery,
before the checks. No other check's message has a side effect on its condition (scanned).

Jobs: GCC at d89528f fails-without (A1) 440d3e; GCC every case at d8453f6 e5a982; GCC ASan+UBSan
every case 3e6bf7; Clang reruns A1 03aa5c, A2 467c0f, A3 5f60a2, A7 597440; suite f8221d; the two
reopen_empty mutants rebuilt on d8453f6 (refs/scratch/journal-group-delete/no-floor-2 journal:5b748cc,
create-from-one-2 journal:a27dfed) 6b2d97, 86e760. The other mutants' tests are unchanged.

## Next

1. Read the jobs above; each mutant log must show a complete build, then the named case failing;
   440d3e must fail with the "c stays closed" line.
2. CI run 45143330663 at d8453f6 (A4 asan/tsan, A6).
3. Then State "ready for review" (durability lens).

## Blocked on

Lead: include/journal/journal.h is outside the file set (R2); widen it or rule otherwise. The suite check (`cmake -B build ...`) lacks -G Ninja; it is the lead's.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 03aa5c (reopen_empty, stays_deleted), GCC e5a982; fails-without 6b2d97 (no-floor), 3473fb (commit-drops-deleted), 86e760 (create-from-one); GCC order bug at d89528f 440d3e |
| A2 | 467c0f (crash, crash_finish), GCC e5a982; fails-without 32f31b (artifacts-first), 70b4bc |
| A3 | 5f60a2 (refused), GCC e5a982 |
| A4 | suite f8221d; GCC ASan+UBSan 3e6bf7; CI asan/tsan run 45143330663 |
| A5 | `docs/mechanisms/host-delete.md`, CHANGELOG.md 2026-11-22 |
| A6 | CI run 45143330663 at d8453f6 |
| A7 | 597440 (loop_thread, owner check compiled in), GCC e5a982 |

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
- R6 CI's gcc failure was the test's: a check whose message recovered the group its condition read
  the status of. Fixed in the test, no code change; the GCC job at d89528f shows it.
