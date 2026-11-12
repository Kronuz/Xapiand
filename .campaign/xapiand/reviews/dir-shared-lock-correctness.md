# dir-shared-lock review: correctness

Commit: completion:d06b0190913ec067b9e0c4f3adedeb06c2f9de1a
Verdict: CHANGES
Date: 2026-11-12

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- 261112-044134-bf64d5: full suite at d06b019, 20/20, including `completion_owner_lock` (matrix with
  the reader in all three topologies, `readers`, `upgrade`, `concurrent_readers`, `interrupted`,
  `unsupported`, `partial_failure`, `base`) and `completion_mutants_anchors`. Enough for A1 and A2 on
  macOS.
- 261112-044138-a4fc2d: owner_lock_test under TSan, `owner_lock: ok`, no reports. It covers the
  two-thread races through one IO and through two, 300 rounds of each of 4 cases, against the
  sequential outcome.
- 261112-044136-c2bcfc (fails-without): `test/mutants/reader_shared_as_exclusive.sh` exits 0 only when
  the test exits 1 and prints `reader then reader: busy, expected acquired`. Its log shows exactly that
  check failing. It exits 2 if the anchor is missing or unapplied, 3 if the build fails and 4 for any
  other outcome. That meets the standard.
- GitHub CI run 43220197978 at d06b019: success on linux gcc/clang/arm64, linux ASan+UBSan,
  linux TSan, FreeBSD, macOS arm64 and x86_64 (A4).

## What I checked myself

- Code paths in `posix.h:668-675, 926-990`:
  1. The claim comes before `flock`.
  2. A failed `lock_shared` unwinds through `~Owner`: `LOCK_UN` on the IO's own descriptor, then the claim is cleared. It cannot free another IO's lock because the claim admits one lock per IO.
  3. `lock_failed` captures errno before `directory_filesystem()` runs.
  4. EINTR is retried. ENOLCK and other errnos are thrown as they are, the same as the exclusive path.
  5. Release order is unlock, then clear the claim.
  6. In `both` mode beside a held reader on the same IO, `claim_file` succeeds and `claim_directory` refuses. The file claim is then released.
- Probe 261112-044718-59b2ef (`.scratch/review-dir-shared-lock/conversion.sh`, `conversion.c`):
  1. Three opens of one directory.
  2. Two hold `LOCK_SH`.
  3. One of those tries `LOCK_EX|LOCK_NB`, which is busy.
  4. The other releases, then a third open tries `LOCK_EX|LOCK_NB`.

  On Darwin 24.6 the third open is still busy. So macOS keeps the shared lock after a failed non-blocking conversion (F2).
- Consumers of the new virtual (Q3), by grepping the tracked sources:
  1. strata overrides `acquire_directory_owner` in a production wrapper (`include/strata/core/counted_io.hh:173`), in `faultfs.hh:581`, and in 9 test wrappers.
  2. prospect does so in 3 test wrappers.
  3. Xapiand has none.
  4. Detent's two `FakeIO`s do not override it.

  All of them still compile. They throw `logic_error` only when the reader is requested, which only strata's unlanded search open does (N1).
- strata's read-only-open design, D1, D4 and D6, against the contract (Q2): N2.
- `tools/mutants.py` anchors and the mutant workflow for Q4: F1.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | gap | Two mechanisms exist where one is enough. (a) `lock_shared` duplicates `lock_exclusive`'s retry loop, and the two differ only in the `flock` operation. (b) `test/mutants/reader_shared_as_exclusive.sh` is a second mutant harness (copy, sed, build, run) beside `tools/mutants.py`, which already does this job. That harness has `--check` anchors in the suite and the `mutants.yml` workflow, and CI never runs the new script. Both duplications come from the file set excluding `tools/`, not from the code's needs. | `posix.h:966-975`; `tools/mutants.py:59,64,70,85,89`; `.github/workflows/mutants.yml:2` | The lead widens the file set to `tools/mutants.py` and `.github/workflows/mutants.yml`, which no other open quest holds. Then: (1) Write one `lock(int fd, int kind, const char* operation, const char* what)` whose loop is `do { result = flock_(fd, kind \| LOCK_NB); } while (...)`. Callers pass `LOCK_EX` or `LOCK_SH`. (2) In `tools/mutants.py`, rewrite the 5 strings that name `lock_exclusive` or the `LOCK_EX` retry: `DIRECTORY_STEP`, the `FILE_STEP` line, `FLOCK_RETRY`, m2's replacement and m3's replacement. Add m15, "reader takes LOCK_EX", anchored on the reader's `lock(directory_, LOCK_SH, ...)` call. Update the count from 24 to 25 in the docstring and the workflow comment. m2 then covers EINTR for both kinds. (3) Delete `test/mutants/`. Test: `completion_mutants_anchors` in the suite, plus one job `python3 tools/mutants.py --only m1,m2,m3,m15` reporting each as killed, tagged `--for A1 --fails-without`. |
| F2 | no | defect | The note says "`flock` itself converts a held lock by dropping it first, so a single-call conversion would have the same window". That holds on Linux, where flock(2) says the existing lock is removed first. On macOS a failed `LOCK_EX\|LOCK_NB` conversion keeps the shared lock. The contract is right: two steps give every platform one behavior. Only the stated reason is platform-wrong, and strata's D4 repeats it. | probe 261112-044718-59b2ef; `docs/mechanisms/directory-locks.md:25` | Replace the sentence with this reason: Linux drops the lock first and a failed non-blocking conversion holds nothing, while macOS keeps it. The claim keeps the IO from converting, so every platform has the same two steps. Fold this into F1's round. |
| N1 | no | gap | The journal's consumer hand-off names faultfs only. strata's production `counted_io.hh:173` also forwards the owner lock and must forward the reader, or a search open through it throws `logic_error`. The same applies to the strata and prospect test wrappers that a search open passes through. `logic_error` is the right default: a forgotten forward fails loudly, rather than silently taking EX or looking like a filesystem limit (`not_supported`). | grep above | Add `counted_io` and the test wrappers to the hand-off for read-only-open. |
| N2 | no | nit | The primitive gives strata D1 (shared lock before the scan, busy beside a writer), D4 (release, then EX non-blocking, then retake on busy) and D6 (a plain open is busy beside a search handle). The window is stated in `directory-locks.md` and in the io.h comment. strata's D4 says "two search handles upgrading at once: one wins". At most one wins, and both lose when neither window overlaps the other's release, as completion's own `upgrade` test shows. | `owner_lock_test.cc` `upgrade`; strata `read-only-open.md:77-78` | Forward to read-only-open: "at most one wins". |
| N3 | no | nit | `PosixFile::try_lease` (`posix.h:552`) is a third `flock` retry loop that is mode-parameterized by `LeaseMode`. It predates this quest and is outside its scope. | `posix.h:545-560` | Hunt row if the owner wants one flock helper. |
| N4 | no | nit | The `Owner` comment line at `posix.h:923` runs past the file's width. `busy_code` also accepts `resource_unavailable_try_again`, which equals EWOULDBLOCK on all three platforms. | `posix.h:923`, `owner_lock_test.cc:570` | Rewrap the line; the second part is optional. |

## Not run

- I did not probe flock conversion on Linux or FreeBSD. For Linux I relied on flock(2); FreeBSD is untested.
- No NFS, SMB or FUSE mount: ENOLCK and ENOTSUP behave like the exclusive path by construction (`lock_failed`). Only the injected-errno tests exercise them.
- Fork inheritance of `directory_` (a shared open file description across fork without exec) predates this quest and is unchanged.
- I did not try F1's refactor.
