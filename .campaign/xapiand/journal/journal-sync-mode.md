# journal-sync-mode: journal

State: waiting on jobs 261124-122708-2f04ab 261124-122713-058d9e 261124-122716-1b2259 261124-122720-bcba79 261124-122735-d94c86 and CI 45480924916
Quest: .campaign/xapiand/quests/journal-sync-mode.md (the lead's; never edit it)

## Doing

Ready commit journal:fe050d1 (pushed; CI run 45480924916). Commits: journal:7f3e548 (pins
completion b4600a8 + faultlab 83fbde7; every PosixIO passes a mode; bench/demo take it;
test/sync_mode_test.cc; T9 plain), journal:3d7481e (docs), journal:fe050d1 (T9 plain checked by
prefix; full: a drive flush after every fsync; bench/flush_mode_ab.sh). Local (Debug, macOS):
the 10 targeted CTest entries pass; T9 plain 39 s, 31,181 checks.

Jobs at fe050d1 unless noted:
- 261124-122708-2f04ab A1,A3,A6: journal_sync_mode_test (host, refusal, table)
- 261124-122713-058d9e A2: T9 full and plain
- 261124-122716-1b2259 A2 --fails-without at mutant journal:d7f0a7d (refs/mutants/journal-sync-mode/full-as-plain-t9): T9 opens full as plain
- 261124-122720-bcba79 A1 --fails-without at mutant journal:d56e937 (refs/mutants/journal-sync-mode/full-as-plain-host): sync_mode_test host opens full as plain
- 261124-122735-d94c86 A5 strict: bench/flush_mode_ab.sh, main journal:4eb8165 vs full vs plain, 7 rounds
- `campaign verify journal-sync-mode --for A1` queued (suite + CI)

- (lead, 2026-11-24) Reviewer job 261124-130552-389d38 exited 1 on three lifecycle coverage checks only: its workload never recycled a segment (reviews/journal-sync-mode-durability.md). Round-2 job 261124-142654-0f4410 recycles segments on real files in each mode and passed; no product failure.

## Next

1. When the jobs finish: Evidence table from `campaign brief`; A5 numbers into
   docs/mechanisms/sync-mode.md ("Cost") and Result; then "ready for review".
2. Merge journal main once detent:journal-bound-first-open lands (no host.h change, ruling (a)).

## Finding (plain mode, for the durability review)

T9 plain, block 1024, seed 1 cut 76: a power loss restored a recycled segment's name
(turns-1 reappears: its rename/unlink was only plain-synced) and recovery kept group 1's
sequence 1 of a turn that wrote 1 and 2, reporting damage. Plain mode can keep part of a turn: a
shorter prefix of the group's log, never a reorder or an unwritten record. 1 of 5,600 crash
points; 0 without reported damage. Documented in docs/mechanisms/sync-mode.md.

## Blocked on

Nothing. The lead ruled (a): the mode stays in PosixIOOptions, a Host follows its IO, host.h does
not change (quest Decisions; Detent has the call shape). Ordering after
detent:journal-bound-first-open stays: merge journal main once it lands.

## Contract for Detent

What Detent's next journal/completion bump must change, at completion:b4600a8:

- `kronuz::journal::PosixIO(directory)` no longer compiles. Every PosixIO takes
  `PosixIOOptions{FlushMode::full}` (Detent's mode):
  `std::make_unique<kronuz::journal::PosixIO>(dir, kronuz::journal::PosixIOOptions{kronuz::journal::FlushMode::full})`.
  Detent builds PosixIOs directly today, contrary to msg-325's "never constructs PosixIOOptions":
  src/detent_authority/service.cc:1835,1847,1849,1851 (journal storage and history), and
  completion PosixIOs in src/detent_local/disk_store.cc:467, src/detent_relay/mirror_service.cc:364,
  benchmarks/, docs/design/async-drivers-bench/. consensus builds journal::Host over
  `std::make_shared<j::PosixIO>(dir)` in bench/netlab/node.cc:211.
- A Host follows its IO: `journal::Host(std::shared_ptr<IO>, Identity, HostOptions)` has no mode
  of its own, and every file the PosixIO opens keeps its mode.

Shapes for "the mode where a host opens" (lead decides; needs host.h, after
detent:journal-bound-first-open):

- (a) Recommended: none in host.h. The PosixIO's required option is already where every host's
  storage is opened, with no default, and it is the one place the mode lives; a Host-level mode
  could only duplicate it (IO has no accessor to check it against, and faultfs's io has none).
- (b) `Host(std::shared_ptr<PosixIO> io, Identity, FlushMode mode, HostOptions = {})`: throws
  std::invalid_argument unless `io->flush_mode() == mode`; the IO-typed constructor stays for
  faultfs. A second statement of the same choice, checked.
- (c) `Host(const std::filesystem::path& directory, PosixIOOptions io, Identity, HostOptions = {})`:
  the host builds its PosixIO; host.h then includes completion/posix.h and becomes POSIX-only.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | <no evidence yet> |
| A2 | <no evidence yet> |
| A3 | <no evidence yet> |
| A4 | [artifact] journal:3d7481e README.md (Highlights, "Flush modes"), ARCHITECTURE.md ("POSIX backend and the durability model"), CHANGELOG.md 2026-11-24, docs/mechanisms/sync-mode.md |
| A6 | <no evidence yet> |
| A5 | <no evidence yet> |

## Result

Not yet.

## Rulings I made

- Tests open PosixIO in `full` (Detent's mode, the stronger); sync_mode_test and T9 run both.
- journal_bench and journal_demo take the mode as a required argument; the bench's comparator is
  renamed the raw loop (it collided with `plain`) and flushes with the call the mode makes for a
  data barrier (completion's `sync_call(data, mode)`).
- T9 sets the macOS calls (`set_flush_mode(mode, sync_platform::macos)`) on every host, so the
  modes differ, and the full-as-plain mutant dies, on Linux CI too. The damage sub-sweeps
  (corrupted blocks, tears, sectors) are mode-independent and run in full only.
- Plain mode's power-loss rule in T9: every group not reported lost recovers a state it was in
  (its acknowledged history or the in-flight turn); and the sweep requires at least one power
  loss to lose an acknowledged record, so a model that kept everything fails it.
- A6: completion's FlushMode stays in posix_sync.h (not io.h), so faultlab keeps its enum; the
  test maps one to the other and checks the two tables equal on every platform.

## History: session 1 (before the re-scope), kept from the first branch

### Doing

Nothing: stopped at the design step, before any code (journal 0 commits ahead of main at 4eb8165a4311).

### Next

1. Lead rules on "Blocked on" below; then this quest bumps the pins and does the journal side.

### Blocked on

Lead (re-scope; touches completion's and faultlab's public API). Facts, at journal main's pins
(completion d8b538c, faultlab 5d4df9b):

- The journal makes no sync system call. Every sync goes through completion's `IO`/`File`
  (`include/journal/host.h:344-347,754`, `primitives.h:52`); `fsync`/`F_FULLFSYNC` are issued by
  completion's PosixIO: `posix_sync.h` `file_sync` (F_FULLFSYNC, raw, not through the seam),
  `files_sync_devices` (device flush), `PosixIO::sync_directory` (directory fsync, then
  F_FULLFSYNC on the owner lock file), `posix.h:421` (owner lock file create), and
  `PosixFile::sync/sync_data` (`posix_file.h:161-162`).
- The turn barrier, the 4,006-call RESTORE path, is a `SyncData` primitive the completion
  backend runs as `file->sync_data()` on the raw PosixFile (`bsd_completion.h:106`, fallback
  worker on macOS), past any IO wrapper the journal could add.
- The crash model's sync semantics are faultfs's (`faultlab/faultfs.h:7-31`); A2 needs faultfs
  to tell a plain sync from a full one.
- The file set has no `CMakeLists.txt` (the completion and faultlab pins), `bench/**`,
  `examples/**` or `fuzz/**`, which construct the IOs a mode would be passed to.

Options:

1. Recommended: the mode belongs to the backend that issues the calls. completion quest: PosixIO
   takes a required `Flush` (`plain` | `full`) at construction, no default; it and the PosixFiles
   and owner lock it opens issue every sync by it, through `SyncCalls` (raw `file_sync` moves
   behind the seam), counted by kind in its tests (A1, A3's refusal and both mutants there);
   decide there how `PosixIOOptions::full_flush_ordering` folds in (D177: plain = fsync
   everywhere; full flushes only with `--full-flush-ordering`). faultlab quest: faultfs's IO
   takes the mode and, in `plain`, a sync survives `kill()`/`power_on()` but not `crash()` (A2's
   model). This quest then: file set + `CMakeLists.txt`, `bench/**`, `examples/**`, `fuzz/**`;
   bump both pins, pass the mode in tests, benches and examples, run the crash sweep in both
   modes, docs (A4), bench (A5). `host.h` does not change: a Host follows its IO's mode, one place
   decides, so the ordering behind detent:journal-bound-first-open goes away.
2. Host-level mode: completion's IO grows a per-call strength (`sync_files(files, Flush)`,
   `File::sync(Flush)`, a flag on the `SyncData` primitive its backends run), faultfs models it
   per call, and the journal threads the host's mode through every call site (journal.h,
   artifact.h, mutation.h, turn_log.h, turn_driver.h, reclamation.h, host.h). Still three repos,
   a wider API, and every IO implementation changes. Not recommended.
3. Journal-only: a decorator that calls `::fsync` on `PosixFile::native_handle()` in plain mode.
   Rejected: duplicates completion's sync logic outside its seam, and misses the backend's
   direct `file->sync_data()` on the turn barrier, the very path RESTORE spends its time in.

### Evidence

No jobs: no code yet.

None for any criterion.

### Result

None yet.

### Rulings I made

None.
