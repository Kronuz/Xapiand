# barrier-warning-strata: journal

State: waiting on jobs 261112-003214-3ac1cf 261112-003216-b8dfe3 261112-003345-52611a
Quest: .campaign/xapiand/quests/barrier-warning-strata.md (the lead's; never edit it)

## Doing

Head strata:d8ae10b8, pushed. Jobs: build-test 261112-003214-3ac1cf (A3, A5); A5 mutant
261112-003216-b8dfe3 (`--fails-without`); A4 261112-003345-52611a (prospect:ea897588); probe 261112-003219-5c8c55 cancelled.

Round 1 at strata:defbdd52, all failed:
- build-test 261112-000616-c46b7e: `faultfs_test.cc:325` in one settle mode. Cause: the test, not
  the fix. With eight sectors written after the mark, "staged bytes alone" needs all eight coins
  (one seed in 512), so 200 seeds missed it. strata:d8ae10b8 writes two sectors (a quarter of the
  surviving crashes). The mutant still yields no outcome 1 or 3 (it tripped the check in all
  three modes, 261112-000619-70e171).
- A4 261112-000625-93e319: prospect's `test/background_merge_test.cc` (`gate_io`),
  `test/directory_test.cc` (`wrapper_io`), `test/index_test.cc` (`flaky_io`) derive from
  `kronuz::journal::IO` and miss completion:c315c03's pure read-only opens: "variable type ... is
  an abstract class" (9 errors). The other 74 tests passed in build-test.

## Next

1. Read the three jobs; on red, fix and resubmit.
3. Paste `campaign brief`'s table; "ready for review" when A3-A5 have evidence.

## Blocked on

Nothing. A4's prospect change is quest prospect-completion-opens (lead, option 1), at
prospect:ea897588 on `quest/xapiand/prospect-completion-opens`; it lands after this quest.

Follow-up for the lead to file: Xapiand's own `kronuz::journal::IO` subclasses need the four
read-only opens (completion:c315c03) at its next repin.

## Evidence

| Criterion | Evidence |
|---|---|
| A3 | build-test 261112-003214-3ac1cf (pending) |
| A4 | 261112-003345-52611a: prospect:ea897588 against strata:d8ae10b8 (pending) |
| A5 | build-test 261112-003214-3ac1cf (`faultfs_test` CB-5 block); mutant 261112-003216-b8dfe3 (pending) |
| A6 | `docs/reviews/calibration.md` row 2026-10-29 (strata, already on main): "Linux CI TSan is the authority for races on struct-copy writes; a local macOS TSan pass is not evidence for that class" |

## Result

<pending the jobs>

## Rulings I made

- R1 Pin completion:e49f288 (the landed `barrier-warning-completion` commit), not main's head
  completion:3a3c205, which only adds `quest/**` to completion's CI triggers.
- R2 Test IOs forward the read-only opens to their backend (completion's own rule for wrapping
  test IOs); strata calls none of them.
- R3 faultfs: `inode::staged` becomes the snapshot of the bytes at the mark.
  A crash with the staged state surviving keeps it whole and applies the per-sector model to
  later writes over it; `sync_directory` and another file's sync settle only the snapshot. The
  no-later-write path consumes no extra coins, so existing seeds keep their outcomes. A stage
  that fails drops its snapshot (as before, its bytes were never staged).
- R4 faultfs read-only opens return a file whose `write_at` and `truncate`
  fail with EBADF; the reusable `_into` forms throw as the base `open_existing_into` does;
  counted_io counts the read-only opens like the others.
