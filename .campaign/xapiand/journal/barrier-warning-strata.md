# barrier-warning-strata: journal

State: blocked on lead: file set lacks include/strata/core/faultfs.hh and include/strata/core/counted_io.hh
Quest: .campaign/xapiand/quests/barrier-warning-strata.md (the lead's; never edit it)

## Doing

Stopped at the file-set boundary. In-set work committed at strata:b21bbb57 (pin bump, test IOs
forward the read-only opens, A5's faultfs test). The out-of-set draft is
`strata-wt/xapiand/barrier-warning-strata/.scratch/out-of-set-b21bbb57.patch` (faultfs.hh,
counted_io.hh); probe job 261112-000131-5608dd runs build-test at b21bbb57 with it applied
(a probe, not evidence).

## Next

1. Once the file set has faultfs.hh and counted_io.hh: `git apply` the draft, commit, `campaign verify --for A3,A5`.
2. A5 mutant: revert the snapshot (staged becomes the whole current bytes again) under `--fails-without`; faultfs_test must fail.
3. A4: prospect suite with `-DFETCHCONTENT_SOURCE_DIR_STRATA=<worktree>` as an integration job.
4. Push the branch for CI (linux-asan-ubsan, linux-tsan).

## Blocked on

Lead: add `include/strata/core/faultfs.hh` and `include/strata/core/counted_io.hh` to the file set.

- A3 needs both. completion:c315c03 (between the old pin completion:e2c6474 and completion:e49f288)
  made the four read-only opens of `kronuz::journal::IO` pure. `faultfs::io` and `counted_io`
  implement none of them, so neither can be instantiated at the new pin, and every suite builds
  on faultfs.
- A5 is a change to `faultfs.hh` itself: `stage()` and `crash()` live there, not under `test/`.
- Options: (a) extend the file set (recommended; both files are test models or IO wrappers, the
  draft is 75 + 9 lines); (b) split the pin into its own quest owning both files. No change to
  `index.hh` or `files.hh` is needed so far.

A6 needs no edit: `docs/reviews/calibration.md` already has the 2026-10-29 row (outside the file set).

## Evidence

| Criterion | Evidence |
|---|---|
| A3 | <pending the file set> |
| A4 | <pending> |
| A5 | <pending the file set>; test at strata:b21bbb57 `test/faultfs_test.cc` (CB-5 block) |
| A6 | `docs/reviews/calibration.md` row 2026-10-29 (strata, already on main): "Linux CI TSan is the authority for races on struct-copy writes; a local macOS TSan pass is not evidence for that class" |

## Result

<pending>

## Rulings I made

- R1 Pin completion:e49f288 (the landed `barrier-warning-completion` commit), not main's head
  completion:3a3c205, which only adds `quest/**` to completion's CI triggers.
- R2 Test IOs forward the read-only opens to their backend (completion's own rule for wrapping
  test IOs); strata calls none of them.
- R3 (draft, out of set) faultfs: `inode::staged` becomes the snapshot of the bytes at the mark.
  A crash with the staged state surviving keeps it whole and applies the per-sector model to
  later writes over it; `sync_directory` and another file's sync settle only the snapshot. The
  no-later-write path consumes no extra coins, so existing seeds keep their outcomes. A stage
  that fails drops its snapshot (as before, its bytes were never staged).
- R4 (draft, out of set) faultfs read-only opens return a file whose `write_at` and `truncate`
  fail with EBADF; the reusable `_into` forms throw as the base `open_existing_into` does;
  counted_io counts the read-only opens like the others.
