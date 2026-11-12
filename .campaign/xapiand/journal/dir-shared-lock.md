# dir-shared-lock: journal

State: ready for review
Quest: .campaign/xapiand/quests/dir-shared-lock.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Nothing: all criteria have evidence at completion:d06b019. GitHub CI run 43220197978 was in
progress at handoff (`campaign brief` tracks it).

## Next

1. Correctness review; fold in findings by id.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261112-044134-bf64d5 (suite), 261112-044138-a4fc2d (owner_lock_test under TSan, no reports); fails without: 261112-044136-c2bcfc (LOCK_EX mutant fails at "separate processes: reader then reader: busy, expected acquired") |
| A2 | 261112-044134-bf64d5 (`base`, `unsupported`, `partial_failure`, `interrupted` in owner_lock_test) |
| A3 | `docs/mechanisms/directory-locks.md` (completion) |
| A4 | 261112-044134-bf64d5 (20/20 tests, incl. completion_mutants_anchors); GitHub CI run 43220197978 |

## Result

`IO::acquire_directory_reader()` (io.h): a virtual whose default throws `std::logic_error`, as
`acquire_directory_owner()`'s does. `PosixIO` takes `flock(LOCK_SH | LOCK_NB)` on its directory
descriptor through the same directory claim as the exclusive lock, and maps errors through one
`lock_failed` (busy EWOULDBLOCK, ENOTSUP/EOPNOTSUPP not_supported). Tests in
`test/owner_lock_test.cc` (reader in the 3-topology pairing matrix, `readers`, `upgrade`,
`concurrent_readers`, EINTR/unsupported/partial-failure/base). Contract and upgrade:
`docs/mechanisms/directory-locks.md`. Mutant: `test/mutants/reader_shared_as_exclusive.sh`.
Consumers to tell when it lands: strata read-only-open (faultfs must forward or model the new
virtual), Detent.

## Rulings I made

- Name `acquire_directory_reader()`, pairing with `acquire_directory_owner()`; a separate virtual
  rather than a shared `OwnerLockOptions` mode, as the quest's contract says, so no backend's
  `lock_owner` sees a new mode.
- One IO holds one directory lock of either kind (the shared lock takes `directory_claimed_`): a
  second shared lock or an exclusive one through the same IO is refused with `std::logic_error`.
  Both `flock` calls would land on the IO's one open file description, where a second converts the
  first and one release frees both. So the upgrade is release-then-acquire, the caller's two steps.
- `lock_exclusive`'s `flock` line and the not_supported test stay verbatim: `tools/mutants.py`
  anchors them (m2, m6, m13; `completion_mutants_anchors` checks each occurs once) and tools/ is
  outside this quest's files. `lock_shared` repeats the 3-line retry loop with `LOCK_SH`; the
  errno mapping is shared in `lock_failed`. A shared-lock mutant in tools/mutants.py is a follow-up
  for whoever owns tools/.
- The reader joins the existing owner_lock_test matrix instead of a new test file (CMakeLists.txt is
  outside the file set and the matrix already covers processes, distinct IOs and one IO).
