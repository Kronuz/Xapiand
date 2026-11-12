# barrier-warning-strata: journal

State: waiting on jobs 261112-000616-c46b7e 261112-000619-70e171 261112-000625-93e319
Quest: .campaign/xapiand/quests/barrier-warning-strata.md (the lead's; never edit it)

## Doing

Head strata:defbdd52 (pin + test IOs at b21bbb57, faultfs snapshot and read-only opens at
defbdd52), pushed for CI. Jobs: build-test 261112-000616-c46b7e (A3, A5); A5 mutant
261112-000619-70e171 (`--fails-without`: crash takes the current bytes whole, settle makes them
durable); A4 prospect 261112-000625-93e319 (prospect:0aa68f4 against defbdd52, churn/dump vs
strata main). Probe 261112-000131-5608dd cancelled.

## Next

1. Read the three jobs; on red, fix and resubmit.
2. CI on the pushed branch (linux-asan-ubsan for the asan check, linux-tsan).
3. Paste `campaign brief`'s table; "ready for review" when A3-A5 have evidence.

## Blocked on

Nothing. (The file set gained faultfs.hh and counted_io.hh, lead msg.)

## Evidence

| Criterion | Evidence |
|---|---|
| A3 | build-test 261112-000616-c46b7e (pending) |
| A4 | 261112-000625-93e319 (pending) |
| A5 | build-test 261112-000616-c46b7e (`faultfs_test` CB-5 block); mutant 261112-000619-70e171 (pending) |
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
