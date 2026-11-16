# barrier-warning-strata review: durability

Commit: strata:d8ae10b8f1dbb31927fe6c01278fc7a476513864
Verdict: APPROVE
Date: 2026-11-12

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- build-test 261112-003214-3ac1cf at d8ae10b8: 75/75, faultfs_test's CB-5 block included (A3, A5).
- A5 mutant 261112-011239-8f21e5: patches both halves of the fix (crash takes the current bytes
  whole; settle makes the flushed bytes durable), exits 1 if faultfs_test passes, exits 0 only on
  the `outcomes.count(1) && outcomes.count(3)` check; log shows `faultfs_test.cc:326` failing in
  all three non-own modes. By reading, each half alone is also killed (m1 by the `none` mode, m2
  by `directory`/`other_file`).
- A4 261112-013520-bd7c93: prospect 27ca2d9 against d8ae10b8, ctest 51/51, churn/dump identical
  to strata main. The earlier 48/51 (prospect ea897588) is pre-existing: attribution job
  261112-011236-77c6e9 fails the same suites against strata main 4c45e3dd.
- CI run 43183755958 green on d8ae10b8 (Linux TSan included, the authority for C2).
- A6: `docs/reviews/calibration.md:18` (already on main) records the macOS TSan blind spot.

## What I checked myself

1. Pin (Q1). CMakeLists.txt:27 pins completion e49f288, on completion origin/main. Between
   e2c6474 and e49f288 `PosixIOOptions` gains `barrier_warning` and `barrier_warning_context`
   appended after `full_flush_ordering` (already present at e2c6474), so positional
   initializers (`directory_test.cc:206`, `admit_all{true, true}`) keep their meaning.
   `order_files_and_directory`'s ordering is unchanged; it now returns whether a barrier only
   ordered and calls the warning once per volume. strata issues barriers only at
   `index.hh:4949` (rollover) and `index.hh:4973` (append_edit), both through `io_`, the host's
   IO; `counted_io.hh:211-213` forwards `order_files_and_directory` to the inner IO, so a host's
   options and warning reach PosixIO. strata builds PosixIO only in tests. No path relies on the
   old ordering.
2. faultfs crash model (Q2), by reading `faultfs.hh:220-255, 365-372, 520-536, 674-686`. After
   `stage()` the staged copy is the `old` side of the per-sector coin for a staged inode whose
   staged state survives, so any subset of post-mark sectors survives over it; when the staged
   state does not survive, `old` is the durable copy (as before). `sync_directory` and another
   file's sync settle only the snapshot (`settle()`), and leave the post-mark write dirty. The
   no-later-write path consumes no extra coins. Remaining approximations are conservative
   (more outcomes than a real barrier allows): a post-mark sector may survive while the staged
   state did not, and the staged intermediate bytes are not an outcome in that branch; neither
   lets an over-trusting protocol pass. Determinism: coins are `std::mt19937_64() & 1`, whose
   output the standard fixes, and inodes iterate in id order, so the 200 seeds give the same
   outcomes on every platform; the rarest required outcome has probability 1/8 per seed (`none`
   mode), so a miss over 200 seeds is about 2.5e-12, and a future change to coin order is
   unlikely to break it.
3. Read-only opens (Q3), probe job 261112-014730-b537b3
   (`.scratch/review-barrier-warning-strata/run.sh`, `probe.cc`, P2): an
   `open_existing_read_only` counts one call; `write_at` and `truncate` fail with EBADF and count
   no mutation (no device op, so no fault rule or power budget is consumed); a media read fault
   and a sync fault both fire through the read-only handle. counted_io's read-only opens use
   the same `open()` helper as the read-write ones (`counted_io.hh:184-189, 227-233`), taking a
   descriptor and a vnode. The test wrappers forward the four opens to their backend.
4. strata's own barrier protocol against the new model: no staged file is written between
   `order_files_and_directory` and its settling sync (`index.hh:4949-4953`; in append_edit the
   log is written after the mark but is never staged), so CB-5's over-trust never applied to
   today's protocol, and the suite at d8ae10b8 passes under the stricter model.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| D1 | no | defect | A sync that fails after its effect, on a staged file written after the mark, makes the staged snapshot durable instead of the flushed bytes: the after-path sets `durable = flushed()` then calls `settle_staged()`, whose `settle()` overwrites it with `*staged` (the success path resets `staged` first, `:514`). Reads then see the staged bytes and a crash keeps them, contradicting the comment at `:496-498`; main's bool model kept the flushed bytes. Conservative (drops an outcome the caller cannot rely on) and unreachable by strata today (point 4). | `faultfs.hh:499-501` with `:102-105`; probe P1 in `probe.cc` (fixed to `nth = 1`; job 261112-014730-b537b3 ran it with the wrong `nth`, so the rule never fired; by reading, a rerun prints `a` for `staged` and `b` for `control`) | `node_->staged.reset()` before `fs_->settle_staged()` in the after-path; a faultfs_test case: stage, write later bytes, `fault{op::sync, 1, EIO, true}`, sync throws, view and crash keep the later bytes. |
| D2 | no | nit | A sync that fails before its effect leaves the inode's snapshot staged while `current` reverts to the durable bytes, so the next `sync_directory` makes the staged bytes durable under reads that show the older ones. A legal outcome (the barrier issued them), but reads and durable disagree until the next write. | `faultfs.hh:495-510` | Optional: reset `staged` there too, or note the case in the header comment. |
| D3 | no | nit | faultfs's `open_reclaim_candidate_read_only` throws ENOENT for an absent name, where `io.h:228-230` says null means absent; it matches faultfs's read-write reclaim open (base default to `open_existing`), so pre-existing in shape. | probe P2 (`reclaim ro missing: throws errno 2`, rw the same) | Optional, outside this quest: override both reclaim opens to return null on ENOENT. |

## Not run

- P1 rerun with the corrected `nth` (one light probe job was the budget); D1 rests on reading
  `faultfs.hh:499-505`, which is unambiguous.
- No TSan or sanitizer locally (Linux CI's TSan is green at d8ae10b8); no Xapiand-side IO
  subclasses (the journal's follow-up for the lead); no timings.
- Did not recheck C1/CB-3 (hardware caveat) or completion's own barrier-warning tests (the
  `barrier-warning-completion` durability review covers them).
