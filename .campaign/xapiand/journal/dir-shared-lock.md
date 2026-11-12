# dir-shared-lock: journal

State: waiting on jobs 261112-045113-23f421 261112-045114-4fb679 261112-045118-31d22a
Quest: .campaign/xapiand/quests/dir-shared-lock.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Correctness review CHANGES folded in at completion:504f409; jobs 261112-045113-23f421 261112-045114-4fb679 261112-045118-31d22a
(suite for A1/A2/A4, `tools/mutants.py --only m1,m2,m3,m15` fails-without for A1, TSan for A1).

## Next

1. Read the jobs; `campaign accept dir-shared-lock correctness --confirm F1=261112-045113-23f421`
   is the lead's; paste the brief's table; State "ready for review".

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | pending at completion:504f409: 261112-045113-23f421 (suite), 261112-045118-31d22a (TSan); fails without: 261112-045114-4fb679 (m1,m2,m3,m15). Superseded at d06b019: 261112-044134-bf64d5, 261112-044138-a4fc2d, 261112-044136-c2bcfc |
| A2 | pending: 261112-045113-23f421 |
| A3 | `docs/mechanisms/directory-locks.md` (completion) |
| A4 | pending: 261112-045113-23f421 and GitHub CI at completion:504f409 (d06b019: run 43220197978 green) |

## Result

`IO::acquire_directory_reader()` (io.h): a virtual whose default throws `std::logic_error`, as
`acquire_directory_owner()`'s does. `PosixIO` takes `flock(LOCK_SH | LOCK_NB)` on its directory
descriptor through the same directory claim as the exclusive lock, and maps errors through one
`lock_failed` (busy EWOULDBLOCK, ENOTSUP/EOPNOTSUPP not_supported). Tests in
`test/owner_lock_test.cc` (reader in the 3-topology pairing matrix, `readers`, `upgrade`,
`concurrent_readers`, EINTR/unsupported/partial-failure/base). Contract and upgrade:
`docs/mechanisms/directory-locks.md`. Mutant: m15 in `tools/mutants.py`.
Consumers to tell when it lands: strata read-only-open, whose `counted_io.hh` (production),
`faultfs.hh` and the strata and prospect test wrappers a search open passes through must forward
the new virtual (N1), and whose D4 should say "at most one wins" (N2); Detent.

## Rulings I made

- Name `acquire_directory_reader()`, pairing with `acquire_directory_owner()`; a separate virtual
  rather than a shared `OwnerLockOptions` mode, as the quest's contract says, so no backend's
  `lock_owner` sees a new mode.
- One IO holds one directory lock of either kind (the shared lock takes `directory_claimed_`): a
  second shared lock or an exclusive one through the same IO is refused with `std::logic_error`.
  Both `flock` calls would land on the IO's one open file description, where a second converts the
  first and one release frees both. So the upgrade is release-then-acquire, the caller's two steps.
- The reader joins the existing owner_lock_test matrix instead of a new test file (CMakeLists.txt is
  outside the file set and the matrix already covers processes, distinct IOs and one IO).

## Review findings (correctness, d06b019)

- F1: one `lock(fd, kind, ...)` for LOCK_EX and LOCK_SH; tools/mutants.py anchors renamed, m15
  (reader takes LOCK_EX) added, 24 -> 25 in the docstring and mutants.yml; test/mutants/ deleted.
  completion:504f409; proof 261112-045113-23f421 (completion_mutants_anchors) and
  261112-045114-4fb679 (m1,m2,m3,m15 killed).
- F2: directory-locks.md states Linux drops the shared lock before converting and macOS keeps it on
  a failed non-blocking conversion (probe 261112-044718-59b2ef); FreeBSD untested. 504f409.
- N1: hand-off in Result names counted_io.hh, faultfs.hh and the strata/prospect test wrappers.
- N2: the note says at most one of two upgrading readers wins; Result forwards it to D4.
- N3: no change (try_lease predates the quest; a hunt row is the lead's).
- N4: the Owner comment rewrapped; `busy_code` kept as is.
