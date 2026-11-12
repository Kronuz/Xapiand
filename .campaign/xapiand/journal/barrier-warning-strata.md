# barrier-warning-strata: journal

State: blocked on lead: A4 needs a prospect change (its three test IOs miss the read-only opens); jobs 261112-003214-3ac1cf 261112-003216-b8dfe3 261112-003219-5c8c55 queued
Quest: .campaign/xapiand/quests/barrier-warning-strata.md (the lead's; never edit it)

## Doing

Head strata:d8ae10b8, pushed. Jobs: build-test 261112-003214-3ac1cf (A3, A5); A5 mutant
261112-003216-b8dfe3 (`--fails-without`); A4 probe 261112-003219-5c8c55 (prospect:0aa68f4 plus
`strata-wt/xapiand/barrier-warning-strata/.scratch/prospect-read-only-opens-0aa68f4.patch`).

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
2. Once prospect carries the patch: A4 job at that prospect commit (evidence).
3. Paste `campaign brief`'s table; "ready for review" when A3-A5 have evidence.

## Blocked on

Lead: A4 needs prospect's three test IOs to forward the four read-only opens (12 lines, the same
shape as strata's test IOs; patch at
`/Users/kronuz/code/Kronuz/strata-wt/xapiand/barrier-warning-strata/.scratch/prospect-read-only-opens-0aa68f4.patch`).
Prospect is another repository, outside this quest. Options: (a) a small prospect quest
landing that patch, then A4 runs at its commit (recommended; prospect's next strata repin needs
it anyway); (b) add prospect's three test files to this quest. Probe 261112-003219-5c8c55 shows
whether the patch is all prospect needs. Xapiand will need the same for any IO subclass of its
own when it repins prospect.

## Evidence

| Criterion | Evidence |
|---|---|
| A3 | build-test 261112-003214-3ac1cf (pending) |
| A4 | blocked on the prospect change; probe 261112-003219-5c8c55 (not evidence) |
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
