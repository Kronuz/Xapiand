# barrier-warning-strata: journal

State: waiting on jobs 261112-011236-77c6e9
Quest: .campaign/xapiand/quests/barrier-warning-strata.md (the lead's; never edit it)

## Doing

Round 2 at strata:d8ae10b8: build-test 261112-003214-3ac1cf passed (A3, A5's test); CI green
(lead). Two red, being resolved:
- A5 mutant 261112-003216-b8dfe3 killed the mutant (`faultfs_test.cc:326` in all three modes)
  but my command did not invert: `--fails-without` only tags, the command itself must exit 0 when
  the test fails. Resubmitted as 261112-011239-8f21e5: exits 0 only if faultfs_test fails with the
  CB-5 outcomes check; a build failure or a surviving mutant exits nonzero.
- A4 261112-003345-52611a (prospect:ea897588 vs strata:d8ae10b8): churn/dump identical; ctest 48
  of 51, failing format13 (`format13_test.cc:603, 676, 783`, expected `verdict::corrupt`), index
  and index_mapped (`index_test.cc:1399` open should throw, `:1795` error text). Attribution job
  261112-011236-77c6e9: prospect:0aa68f4 against strata main 4c45e3dd (cold-tier-step2, never run
  by prospect). prospect:ea897588 cannot build at main's old completion pin (its `override`s name
  opens main's completion lacks); its patch only adds forwarding, so 0aa68f4 runs the same tests.
  Main failing the same three: pre-existing, the lead files it. Only this branch failing: this
  quest's defect.

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
| A3 | build-test 261112-003214-3ac1cf (passed); CI green at d8ae10b8 |
| A4 | 261112-003345-52611a: churn/dump identical, ctest 48/51; attribution 261112-011236-77c6e9 (pending) |
| A5 | build-test 261112-003214-3ac1cf passed (`faultfs_test` CB-5 block); mutant 261112-011239-8f21e5 killed (`faultfs_test.cc:326` in all three modes); b8dfe3 killed it too but did not invert its exit |
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
