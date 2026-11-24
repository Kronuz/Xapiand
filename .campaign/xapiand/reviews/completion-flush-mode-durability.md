# completion-flush-mode review: durability

Commit: completion:113cee91991e567729554d14f5716fa226e46e68
Verdict: CHANGES
Date: 2026-11-24

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

The mechanism is right. One table (`sync_call`) chooses every macOS call. Full mode is the old
durable behavior plus `full_flush_ordering`, syscall for syscall, and it refuses on a real
filesystem instead of degrading. Every spelling that builds a `PosixIO` names a mode or passes a
`FlushMode::Value`. Two things stand in the way. First, a full-mode file loses its `F_FULLFSYNC`
when a plain-mode backend batches it (F1). Second, no test notices if a reusable file stops
carrying its backend's mode (F2).

For Detent, which passes `full`: every sync of its own backend's files is today's durable
behavior with `full_flush_ordering` (P5), and fails rather than degrades (P2). F1 reaches Detent
only if a plain backend in the same process batches Detent's files.

## Evidence I relied on

- 261124-091927-b0b7d9: build-test at 113cee9, Release, 25 of 25 tests pass on this Mac.
- The contributor's mutants (`tools/mutants.py` at 113cee9), each `--fails-without` and killed:
  f1 261124-091906-a47141 (plain keeps `F_FULLFSYNC` for `File::sync_data`; `sync_calls_test`),
  f2 261124-091911-31b2b6 (plain keeps `F_FULLFSYNC` in `sync_directory`; `sync_calls_test`),
  f3 261124-091915-b8aea1 (full issues `fsync`; `sync_files_test`), f4 261124-091919-537673
  (plain keeps `F_FULLFSYNC` for `File::sync`, `owner.lock` and a batch's devices;
  `sync_files_test`, "a plain durable batch is its fsyncs"), f5 261124-091923-20e986 (full falls
  back to `fsync` where `F_FULLFSYNC` is unsupported; `sync_calls_test`, "File::sync fails with
  the full flush's errno").
- 261124-091935-9baf66: `full_flush_ordering` is gone from code, tests, benches and examples (A3).
- CI run 45447191872 at 113cee9: success on linux-x86_64-gcc, linux-x86_64-clang,
  linux-arm64-gcc, linux-asan-ubsan, linux-tsan, freebsd-x86_64, macos-arm64 and macos-x86_64
  (the headers matrix job was skipped). On Linux and FreeBSD, `test/sync_calls_test.cc:444-500`
  asserts the exact call sequence through the seam in both modes, with `fdatasync` for
  `sync_data` on Linux. That run is my evidence for the Linux and FreeBSD rows.

These cover the table at every single-file site, both modes' batches on one backend, and full
mode's refusal through the seam. They do not cover a batch that mixes backends, reusable files,
the real syscalls (all of the above goes through the seam), or old-versus-new equality. My probes
cover those.

## What I checked myself

The probes live in `.scratch/xapiand/review-completion-flush-mode-durability/probe/`. I built them
against exports of 113cee9 (`src/`) and 02ab67f (`src-base/`) and ran them locally under
`quiet light` on this Mac (macOS 15.8.1, x86_64, APFS). Outputs are in `out/`.

- **P1** `probe_cross_backend.cc`: a full-mode backend's file, synced by a plain-mode backend,
  with both seams recording and forwarding to the real calls (F1).
- **P2** `probe_devfs_refusal.cc`: both modes on devfs. devfs is a real filesystem whose
  `F_FULLFSYNC` and `F_BARRIERFSYNC` fail (`ENOTSUP`; `ENODEV` on `/dev/null`) while its `fsync`
  succeeds. It runs with the seam and with the real calls only.
- **P3** `probe_mode_value.cc` and **P4** `api_shape.py`: what a mode nobody chose becomes, and 35
  spellings compiled against the headers (F3).
- **P5** `oracle_scenario.cc`, `interpose.c` and `run_oracle.sh`: a frozen-copy oracle. One
  scenario of 31 steps runs every sync entry point: `File::sync`, `File::sync_data`, the three
  batches, `sync_directory` with and without `owner.lock`, the creation of `owner.lock`, a
  reusable file, a foreign file, a second backend's file, and each sync primitive through both
  `drive_synchronously` and the native queue. A `DYLD_INSERT_LIBRARIES` interposer logs every
  real `fsync`, `fdatasync`, `F_FULLFSYNC` and `F_BARRIERFSYNC`, from any path, seam bypassed.
  The scenario runs against 02ab67f with `full_flush_ordering = true` and against 113cee9 in each
  mode.
- **P6** `run_mutants.py`: six mutants that f1 to f5 do not reach (g1 to g6); a mutant the three
  sync tests miss then runs against the whole suite.
- **P7** `probe_table_oracle.cc`: completion's `sync_call` against faultlab's `call_for`
  (faultlab:83fbde7, `include/faultlab/faultfs/types.h:75-87`), the table faultfs's crash model
  uses, over all 24 rows (4 sites, 2 modes, 3 platforms).

Two campaign jobs at 113cee9 rerun the same probes in their own checkout, for the record:

- 261124-100711-9575f2 runs P1 to P5 and P7 (`probe/job.sh`). It exits 1 while P1 finds a
  degraded path, and its `SUMMARY` line gives each probe's exit. It failed as expected and
  reproduced every local result: `p1_cross_backend=1 p2_devfs_refusal=0 p3_mode_value=0
  p4_api_shape=0 p7_table_oracle=0; P5 oracle exit 0`.
- 261124-100715-631bb1 runs P6 (`probe/mutants_job.sh`). It exits 1 while a mutant survives. It
  failed as expected and reproduced the local run: g1 to g5 are killed, and g6 survives the
  three sync tests and the whole suite (`100% tests passed out of 22`).

Rerun both at the fixed head. P1 does not count a refused batch as degraded, so it passes under
F1's recommended fix and under its refusal option. If the fix moves a line that a g mutant
anchors on, `run_mutants.py` reports it, and the anchor needs updating.

### The review questions

1. **No path gives a full backend's own files a plain sync, except a batch run by another
   backend (F1).** Every sync syscall site at 113cee9, and what chooses its call:

   | Site | Call | Chosen by |
   |---|---|---|
   | `PosixFile::sync`, `sync_data` (`posix_file.h:162-163`) | `file_sync` → `SyncCalls::issue` (`posix_sync.h:148-153`) | `sync_call(file or data, the file's own mode)` |
   | creating `owner.lock` (`posix.h:420`) | `file_sync` | `sync_call(file, backend mode)` |
   | `sync_directory` (`posix.h:275-286`) | `fsync` of the directory, then `issue` through `owner.lock` or the directory | `sync_call(directory, backend mode)` |
   | macOS batch (`posix_sync.h:164-214`, from `posix.h:254` and `posix.h:321`) | staged `fsync` of each file and the directory (both modes), then one call per device | `sync_call(file or order, backend mode, macos)` (`posix_sync.h:166-167`) |
   | plain barrier (`posix_sync.h:115-121`) | `F_BARRIERFSYNC`; on `ENOTSUP`, `EOPNOTSUPP` or `ENOTTY`, `F_FULLFSYNC` | reached only from `sync_call(order, plain)` |
   | Linux and FreeBSD batch (`posix_sync.h:220-234`) | writeback hint (Linux), `fsync` each | the mode is not read; this is the table's `fsync` in both modes |
   | io_uring `Sync`, `SyncData`, `DirectorySync` (`linux_completion.h:278-292`) | `IORING_OP_FSYNC`, `IORING_FSYNC_DATASYNC` | the mode is not read; equals the Linux row |
   | FreeBSD AIO `Sync`, `SyncData` (`bsd_completion.h:105-107`, `133`) | `aio_fsync(O_SYNC)`, `aio_fsync(O_DSYNC)` | the mode is not read; both modes the same (F5) |

   On macOS, `sync_call` chooses every call except the staged `fsync`s, which both modes issue
   before the device's call. On Linux and FreeBSD, the batch and the native queues sync without
   reading the mode. In both modes they issue what `sync_call` names there, except FreeBSD's
   native `SyncData`, whose `aio_fsync(O_DSYNC)` is a data sync (F5). The mode is required at
   construction (P4). Every `PosixFile` copies its backend's `SyncCalls` when it is made:
   `posix.h:465` for opened and created files, `posix.h:360` for reusable ones. The mode never
   changes after that: `replace_sync_calls` swaps only the system calls (`posix.h:151-166`).
   Retries loop on `EINTR` with the same call, and any other error fails the call. P5 shows the
   real syscalls in each step: full issues `F_FULLFSYNC` wherever the old code did, and plain
   issues none. The exception is a batch run by another backend. `files_sync_devices` reads the
   batching backend's mode, not the file's, so a full file batched by a plain backend gets
   `fsync` only (P1, F1). P7: all 24 of `sync_call`'s rows match faultfs's `call_for`.
2. **Full keeps today's refusals, shown on a real kernel.** On devfs (P2), full
   `sync_directory`, `sync_files_and_directory`, `order_files_and_directory`, `File::sync` and
   `File::sync_data` each fail with `F_FULLFSYNC`'s errno. Nothing is issued after the failed
   call, with the seam or with the real calls only, though devfs's `fsync` would have succeeded.
   f5 covers `issue`'s fallback. The batch's device flush calls `full_sync` directly
   (`posix_sync.h:207`), where f5 does not reach. My g1 adds the fallback there, and
   `sync_calls_test` kills it. **Full is the old durable behavior plus `full_flush_ordering`,
   syscall for syscall** (P5): 72 calls over 31 steps and an empty diff
   (`out/oracle_old_vs_full.diff`). The one difference I found is not a call: a failed
   `File::sync_data` on macOS now throws "durably sync journal data", where it used to throw
   "durably sync journal file". My g2 restores the old default (`F_BARRIERFSYNC` at full's
   ordering points), and three tests kill it.
3. **Plain on macOS: durable syncs are `fsync`; an ordering point is `fsync` of each file and the
   directory, then one `F_BARRIERFSYNC` on the directory's device** (P5: 58 calls, no
   `F_FULLFSYNC`, 7 barriers, all at ordering points). A file on another device keeps only its
   `fsync`, as `flush-mode.md:13` states. Where `F_BARRIERFSYNC` is unsupported, plain issues
   `F_FULLFSYNC` instead. Where that is unsupported too, the ordering call fails with its errno,
   while plain's durable calls still succeed (P2 on devfs). `flush-mode.md:18` and
   `commit-barriers.md` ("Fail closed") state this; the README table and the CHANGELOG do not
   (F4). The Linux and FreeBSD rows hold at the seam (CI's `sequence` test, both modes, the Linux
   seam with `fdatasync`). The native queues do not read the mode and issue the same calls in
   both modes. One doc sentence about FreeBSD's AIO is wrong (F5).
4. **No `PosixIO` builds unless its spelling names a mode, except through `FlushMode::Value`
   (F3).** P4 compiles 35 spellings against the headers. All 25 spellings without a mode are
   refused: each constructor with and without context and artifact mode, `make_posix_io`,
   `make_shared`, `make_unique`, `PosixIOOptions{}` and `PosixIOOptions o;`, `{true, true}`,
   designated initializers without `.flush_mode`, `.flush_mode = {}`, an aggregate holding
   options, `optional::value_or(PosixIOOptions{})`, and `FlushMode` from nothing, `{}`, `{0}`,
   `{true}` and `= 1`. The old argument order (artifact mode before options) is refused, and
   `PosixIO` is `final`, so no subclass adds a default. The 6 spellings the migration names
   build. Taking the options before the artifact mode reads well, and the CHANGELOG names the
   change. Two spellings build without naming a mode, and both are `plain`:
   `{FlushMode::Value{}}`, and a value-initialized `FlushMode::Value` in a caller's struct. The
   public enum's zero is `plain`, so a caller's own options can carry a mode nobody chose (F3).

P6: g1 to g5 are killed. g3 (`sync_directory` ignores the mode), g4 (a batch never flushes a
device) and g5 (`owner.lock` syncs plain) are each killed by two or three tests. g6 survives the
whole suite (F2).

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | defect | A full-mode file synced by a plain-mode backend gets `fsync` only. This happens on four paths: `sync_files`, `sync_files_and_directory`, `order_files_and_directory`, and the `Sync` primitive, which is `io.sync_files` (`operation.h:159`). `files_sync_devices` takes the device call from the batching backend's mode (`posix_sync.h:166-167`, called from `posix.h:254` and `posix.h:321`) and never reads the file's own `calls_`. That contradicts `io.h:274` ("Makes every file durable, as File::sync on each would"), `flush-mode.md:11` ("Files carry it") and `flush-mode.md:18` ("Full mode has no fallback"). Cross-backend batches are supported and tested (`test/sync_files_test.cc:270`, `examples/demo.cc:31`). `SyncData` and `sync_data_files` follow the file's mode, so the same file gets `F_FULLFSYNC` from one primitive and `fsync` from the other. A full backend batching a plain file gives it `F_FULLFSYNC`, which is stronger and harmless. faultfs keeps one mode per filesystem, so a crash-model run cannot show this. | P1: `out/p1_cross_backend.txt`, five cases on four paths, `RESULT: a full-mode file got a plain sync on 5 path(s)`; job 261124-100711-9575f2 (exit 1, `p1_cross_backend=1`) | Recommended: a `PosixFile` whose mode is stronger than the batching backend's syncs on its own, through `file->sync()`, as a foreign file already does (`posix.h:251`, `posix.h:316`). Or its device takes the stronger call. Test: with both backends recording, a full backend's file batched by a plain backend records `F_FULLFSYNC` on each of the four paths. The test fails at 113cee9. Other options are the owner's call: refuse a batch whose files carry another mode (`std::invalid_argument`), or keep the behavior and state in `io.h:274` and `flush-mode.md:11` that the batching backend's mode decides. |
| F2 | yes | gap | No test checks a reusable file's sync calls. `ReusableFile` copies its backend's `SyncCalls` when `make_closed_file` makes it (`posix.h:360`). `install` sets only the descriptor (`posix_file.h:134`), so this copy is the only place a reusable file gets its mode. Mutant g6 builds the copy in plain mode, and the whole suite passes with it: all 22 tests of the build without the asio and task extensions, the three sync tests included. Reusable files back the `CreateInto`, `OpenInto` and `OpenCandidateInto` primitives (`operation.h:91-104`), which is question 1's reopen. The code is correct today (P5: in full mode a reusable file's `File::sync` and `File::sync_data` are `F_FULLFSYNC`). Nothing in the suite would catch a regression. | P6: `out/mutants_g6_run.txt`, "SURVIVED g6 ... the three sync tests and the whole CTest suite pass with it"; job 261124-100715-631bb1 (exit 1, `RESULT: 5 of 6 killed`) | Test: in each mode, `make_closed_file` plus `create_exclusive_into` (and `open_existing_into`), then `File::sync` and `File::sync_data`, recording the mode's calls. Fails without: g6 (`probe/run_mutants.py g6`). |
| F3 | no | nit | `FlushMode` refuses a missing mode, but its public `FlushMode::Value` enum has `plain` at zero. A caller's options struct holding a `FlushMode::Value` gets `plain` if value-initialized, zeroed as a global, or `memset`, and passes it on with nobody choosing. `sync_call`'s macOS row tests only `== full`, so out-of-range values (2, 255) also issue plain's calls. The failure is silent and lands on the weaker mode. | P3: `out/p3_mode_value.txt`; P4's last two rows build | The owner's call. Options: give the enum no zero (`plain = 1`, `full = 2`) and refuse zero; or make `sync_call` treat anything but `plain` as `full`; or keep it. |
| F4 | no | nit | The README table (`README.md:80`) and the CHANGELOG (`CHANGELOG.md:23`) say plain issues "one `F_BARRIERFSYNC` per ordering point". Where the filesystem lacks the barrier, plain issues `F_FULLFSYNC` instead, and the ordering call fails if that is unsupported too. Only `flush-mode.md:18` and `commit-barriers.md` say so. | P2 (plain on devfs) | Add one clause to each. |
| F5 | no | nit | `ARCHITECTURE.md:42` says FreeBSD's AIO issues its own syncs for `DirectorySync`. It syncs only `Sync` and `SyncData` natively (`bsd_completion.h:105-107`); `DirectorySync` goes through `PosixIO::sync_directory`. The table's FreeBSD cell for `File::sync_data` (`SyncData`) says `fsync`, while the native `SyncData` is `aio_fsync(O_DSYNC)` (`bsd_completion.h:133`). Durability is unaffected: `O_DSYNC` flushes the data, and both modes are the same. | `bsd_completion.h:105-107`, `133`; `ARCHITECTURE.md:35`, `42` | Correct the sentence and the cell. |
| F6 | no | nit | `io.h:274` and `io.h:284` still say a batch "makes every file durable". In plain mode on macOS, durable means it survives an application or kernel crash, not a power loss. A journal user who reads only the IO contract does not learn this. | `io.h:274`, `io.h:284` | Add one line in `io.h` that points at `FlushMode`. |
| F7 | no | nit | Each `PosixFile` now carries its own `SyncCalls`, so on macOS x86_64 it grows from 16 to 48 bytes. That is 32 bytes per open file, or 32 MB per million open files on a many-indexes host. | P5 and P3: `sizeof` lines in `out/oracle_run.txt` and `out/p3_mode_value.txt` | The owner's call: store a mode byte and a pointer to a shared seam table, or keep it. |

Blocking: yes or no. Kind: defect (wrong behavior), gap (unproven claim or missing test), nit. A
failing probe is blocking until root-caused to a finding whose fix has a fails-without test. A
finding that would remove or narrow user-visible behavior offers options, including keeping it,
and is never blocking.

Notes: I am a subagent of the lead's session, not a session of my own titled `xapiand · reviewer
completion-flush-mode durability`. The local runs and the two jobs run the same probe sources at
the same commit.

## Not run

- Power loss on any hardware, and a crash-model run of either mode. faultfs models both modes
  (faultlab:eefba53, faultlab:83fbde7); I checked only that its table matches completion's (P7).
  It has one mode per filesystem, so it cannot show F1.
- Linux and FreeBSD locally. CI's run 45447191872 covers them. P1, P2 and P5 test macOS calls
  only; both modes make the same calls on Linux and FreeBSD.
- macOS arm64 locally: CI's macos-arm64 job ran the suite, but my probes ran on x86_64 only.
- Filesystems other than APFS and devfs: HFS+, FAT, exFAT, SMB, NFS and FUSE volumes.
- The cost of either mode: not measured.
- The external callers' migrations (journal-sync-mode, Detent, Xapiand). P4 shows only that they
  cannot build without naming a mode.
