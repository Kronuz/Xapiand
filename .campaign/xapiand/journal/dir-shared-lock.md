# dir-shared-lock: journal

State: waiting on jobs 261112-044134-bf64d5 261112-044136-c2bcfc 261112-044138-a4fc2d
Quest: .campaign/xapiand/quests/dir-shared-lock.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Jobs at completion:d06b019: build-test (A1, A2, A4), the LOCK_EX mutant (A1 --fails-without),
owner_lock_test under TSan (A1). GitHub CI on the pushed branch (A4).

## Next

1. Read the three jobs and GitHub CI for completion:d06b019; paste `campaign brief`'s table; State
   "ready for review".

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | pending: 261112-044134-bf64d5 (suite), 261112-044136-c2bcfc (mutant), 261112-044138-a4fc2d (TSan) |
| A2 | pending: 261112-044134-bf64d5 |
| A3 | `docs/mechanisms/directory-locks.md` (completion) |
| A4 | pending: 261112-044134-bf64d5, GitHub CI |

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
