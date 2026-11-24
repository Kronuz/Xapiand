# completion-flush-mode review: durability (re-review, fix round 1)

Commit: completion:b4600a82a8d30d4f63541d9c322a4ac5eceaee95
Verdict: APPROVE
Date: 2026-11-24

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

I reviewed the whole sync mechanism at b4600a8, not only the diff since 113cee9. On every path a
file gets its own mode's call or a stronger one. Full mode fails instead of degrading. The seven
first-review findings are closed. The new seam table works: it keeps a seam for a file that
outlives its backend, it is race-free under TSan, and it throws on the 256th seam. Its tables are
per image, not per process (N2), which affects test seams only. The 3 tests the quest slot's
build-test skipped pass in a fresh build (N1). Nothing blocks.

For Detent, which passes `full`: every sync of a full backend's own files is the old durable
behavior plus `full_flush_ordering`, call for call (P5). A plain backend batching Detent's files
now gives each one `F_FULLFSYNC` (P1, `cross_backend`). Where `F_FULLFSYNC` fails, the call fails
and is never retried as `fsync` (P2, g1).

## Evidence I relied on

The code at b6f44b5 is the same as at b4600a8; the only difference is `test/check_seam.py`
(`git diff --stat b6f44b5 b4600a8`), so jobs at b6f44b5 count for this commit.

- 261124-111516-8fc497: build-test at b4600a8, 22 of 22 pass. It is the quest slot's build with
  `COMPLETION_TASK` and `COMPLETION_ASIO` off (N1); R2 runs the other 3 tests.
- Mutants f6 to f9, each a one-line diff on b6f44b5 (checked with `git diff`), each built, and each
  failed `completion_sync_primitives_test` on the intended check:
  - f6 261124-110320-b7760e: "SyncFiles of (full) files in a (plain) batch ... got: fsync:a fsync:d
    fsync:b".
  - f7 261124-110324-f3f38e: "File::sync of a reusable file after create_exclusive_into (full) got:
    fsync:r".
  - f8 261124-110328-fbd236: "a FlushMode refuses a Value other than plain or full".
  - f9 261124-110332-cae4a1: "a PosixIO refuses options whose mode is neither plain nor full".
- f1 to f5 ran at 113cee9 (261124-091906-a47141 to 261124-091923-20e986). I count them here because
  the lines they change (`sync_call`'s rows, `issue`'s cases) are unchanged in the 113cee9..b4600a8
  diff, their anchors still match once each (`completion_mutants_anchors` passes in 8fc497 and in
  R2), and the tests that killed them have only been added to.
- 261124-110339-12e18b, the first reviewer's P1 to P5 and P7 at b6f44b5, all exit 0:
  - P1: no plain sync reached a full file, 0 paths.
  - P2: on devfs, full mode fails with `F_FULLFSYNC`'s errno and issues nothing after it.
  - P3: 0, 255 and an unchosen `Value` are refused, and `sizeof(PosixFile)` is 16.
  - P4: 35 of 35 spellings behave as listed.
  - P5: new `full` makes exactly the 72 calls of old+`full_flush_ordering` over 31 steps; `plain`
    issues no `F_FULLFSYNC` and only 7 barriers.
  - P7: all 24 rows agree with faultfs's `call_for`.
- 261124-110342-c05f38: P6 at b6f44b5; g1 to g6 are all killed, g6 by the new `reusable` test.
- The three adaptations in `.scratch/xapiand/completion-flush-mode/probe/` (`diff -r` against the
  first reviewer's copy; every other file is byte-identical) do not weaken any assertion:
  - P2 installs the same two recording functions through `intern_sync_seam` instead of two pointer
    fields, in the same mode, and its `expect`s are unchanged.
  - P3 always exits 0 and only reports. It now prints the refusal its constructions throw. The
    refusal itself is asserted by `flush_mode_values` and killed f8 and f9.
  - g5's replacement copies `sync_calls_` and sets `plain`, which is the same mutant as the
    original, which copied both pointers. It is still killed by two tests.
- 261124-111527-641d3d (A3): `full_flush_ordering` is gone at b4600a8.
- CI run 45468242235 at b4600a8 succeeds on freebsd-x86_64, linux-x86_64-gcc, linux-x86_64-clang,
  linux-arm64-gcc, linux-asan-ubsan, linux-tsan, macos-arm64 and macos-x86_64 (the headers job is
  skipped). The logs show `completion_sync_calls` and `completion_sync_primitives` passing on every
  one of them. On Linux and FreeBSD these tests assert the exact call sequences in both modes,
  including the new `cross_backend` and `reusable` rows.

## What I checked myself

My scripts are in `.scratch/xapiand/review-completion-flush-mode-durability-r2/`. Rerun them as
their headers say.

- **R2** `fresh_suite.sh`, job 261124-112648-36eb3f, exit 0: a fresh checkout with a fresh build
  directory and task and asio on. 46 targets build and 25 of 25 tests pass, including
  `completion_seam`, `completion_asio_extension` and `completion_asio_seam`.
- **R1 and R3** `job.sh` (`probe_seam_table.cc`, `seam_image.cc`, `probe_seam_images.cc`), job
  261124-113533-ed160a, exit 0. All recorders are non-forwarding.
  - exhaust: 255 distinct seams intern to 1..255 in order. The 256th throws `std::length_error` and
    the count stays at 256. Known seams and the default still intern after that. A `PosixIO` whose
    `replace_sync_calls` threw keeps its previous seam, both for its new files and for its own
    `sync_directory`.
  - outlive: files of a destroyed full backend still sync through its seam with `F_FULLFSYNC`, the
    reusable file included. A later backend's seam and mode do not reach them.
  - isolation: each file syncs through its own backend's seam and in its own mode. One seam index is
    shared by a full and a plain backend without sharing their modes. `replace_sync_calls` reaches
    only files opened after it. A plain batch, durable or ordered, leaves a full file to its own
    seam and `F_FULLFSYNC`.
  - concurrent: 8 threads intern 200 new and repeated seams while syncing 1199 files taken from a
    pool that every thread fills, with the files and their indices handed over through a mutex. There are 0 mismatches, both in the
    plain build and under ThreadSanitizer, with no report. Its runtime is linked (`otool -L`).
  - images (R3): two shared libraries that each compile completion in hold two tables, whether
    built with hidden or with default visibility (dlopen'ed with `RTLD_LOCAL`). Each image hands
    out index 1 for a different seam (N2). An entry no test interned is the system calls.
- **R1's teeth** `tsan_teeth.sh`, job 261124-113650-b8d12a, exit 0: with `intern_sync_seam`'s
  `lock_guard` deleted, the concurrent case reports "ThreadSanitizer: data race posix_sync.h:142 in
  intern_sync_seam" (a line number in the mutant copy). So the clean run above is evidence.

### The review questions at b4600a8

1. **No path gives a full file a plain sync.** Every site that issues a sync at b4600a8, and what
   chooses its call:

   | Site | Call | Chosen by |
   |---|---|---|
   | `PosixFile::sync`, `sync_data` (`posix_file.h:164-165`) | `file_sync` → `SyncCalls::issue` (`posix_sync.h:190-195`, `172-186`) | `sync_call(file or data, the file's own mode)` |
   | creating `owner.lock` (`posix.h:421`) | `file_sync` | the backend's mode |
   | `sync_directory` (`posix.h:269-280`) | `fsync` of the directory, then `issue(sync_call(directory))` through `owner.lock` or the directory | the backend's mode |
   | `batch_fds` (`posix.h:285-297`), the one entry of `sync_files` (`posix.h:246-249`) and `sync_staged` (`posix.h:318-323`) | another backend's file, or a `PosixFile` the batch would weaken, runs its own `File::sync` first | `batch_weakens(file's mode, batch's mode)` (`posix_sync.h:95-97`) |
   | macOS batch `files_sync_devices` (`posix_sync.h:206-256`) | a staged `fsync` of each file and the directory, then one call per device | `sync_call(file or order, the batch's mode, macos)` (`posix_sync.h:208-209`), never weaker than a batched file's own mode |
   | plain barrier (`posix_sync.h:158-164`) | `F_BARRIERFSYNC`; where unsupported, `F_FULLFSYNC` | plain's order site only |
   | Linux and FreeBSD batch (`posix_sync.h:262-276`) | writeback hints (Linux), then `fsync` each | the mode is not read; this is the table's row in both modes |
   | io_uring `Sync`, `SyncData`, `DirectorySync` (`linux_completion.h:278-292`) | `IORING_OP_FSYNC` (`DATASYNC` for `SyncData`) | the mode is not read; matches the Linux row |
   | FreeBSD AIO `Sync`, `SyncData` (`bsd_completion.h:105-107`, `133`) | `aio_fsync(O_SYNC)`, `aio_fsync(O_DSYNC)` | the mode is not read; the same in both modes |

   - Defaults: `FlushMode` has no default constructor and its `Value` has no zero. The constructor
     throws on any value other than `plain` or `full` (`posix_sync.h:49-57`), and `PosixIO` refuses
     options whose bytes hold anything else (`posix.h:90-93`). Evidence: `flush_mode_values`, f8,
     f9, P3 and P4.
   - Reopen: a reusable file takes its mode at `make_closed_file` (`posix.h:361`), and only its own
     backend can fill it. `open_existing` and the reclaim opens build a new file in the opener's
     mode (`posix.h:465-468`). Evidence: `reusable`, f7, g6 and R1 outlive.
   - The device flush: `batch_fds`. Evidence: `cross_backend` in both directions, through the IO
     methods, `drive_synchronously` and the native queue; f6; P1; and R1 isolation, which also
     covers the ordered batch.
   - The directory barrier (g3), the lock file (g5) and the turn barrier's direct `sync_data` (the
     file's own mode; P1's `x->sync_data()`) are all covered.
   - A batch still uses the batching backend's seam for every descriptor it batches, as at 02ab67f.
     That is the only way a file meets another backend's seam, and it matters to test seams only.

2. **Full mode still refuses, shown on a real kernel.** P2 at b6f44b5 runs full mode on devfs.
   `sync_directory`, both batches, `File::sync` and `File::sync_data` each fail with
   `F_FULLFSYNC`'s errno, and nothing is issued after the failure, though devfs's `fsync` would
   succeed. The batch's device flush has no fallback (g1 is killed). `issue`'s `full_fsync` case
   is unchanged since f5 killed its fallback. A weakened file's self-sync is `File::sync`, the same
   `issue`. The seam table cannot route a sync to a non-system call in production: index 0 and
   every entry no test interned is the default seam (R3).

3. **The Linux and FreeBSD claims hold.** `sync_call`'s Linux row (`fsync`, and `fdatasync` for
   data) and its other row (`fsync`) ignore the mode (`posix_sync.h:86-91`). `batch_weakens` is
   false off macOS (`table_rows`, `sync_primitives_test.cc:324-332`). The batch and both native
   queues never read the mode. CI 45468242235 passes these assertions on Linux x86_64 and arm64,
   on FreeBSD, and under ASan/UBSan and TSan. README 81-82, ARCHITECTURE 34-42 and flush-mode.md
   15 and 39 match the code. Two older mechanism docs do not (N3). I did not verify flush-mode.md
   5's background claim that FreeBSD's `fsync` flushes the drive cache ("Not run").

### First-review findings at b4600a8

| ID | Closed | Where | Shown by |
|---|---|---|---|
| F1 | yes | `batch_fds` (`posix.h:285-297`) with `batch_weakens` (`posix_sync.h:95-97`) is the one entry of every PosixIO batch | `cross_backend` (`sync_primitives_test.cc:420`); f6 261124-110320-b7760e; P1 in 261124-110339-12e18b (0 paths); R1 isolation |
| F2 | yes | `reusable` (`sync_primitives_test.cc:452`), both modes, after `create_exclusive_into`, `open_existing_into` and `open_reclaim_candidate_into` | f7 261124-110324-f3f38e; g6 in 261124-110342-c05f38 |
| F3 | yes | `Value { plain = 1, full = 2 }` and a throwing constructor (`posix_sync.h:49-57`); `PosixIO`'s byte check (`posix.h:90-93`) | `flush_mode_values` (`sync_primitives_test.cc:340`); f8 261124-110328-fbd236; f9 261124-110332-cae4a1; P3 |
| F4 | yes | `README.md:80`, `CHANGELOG.md:22-24` state the barrier's `F_FULLFSYNC` fallback and its failure | [artifact] |
| F5 | yes | `ARCHITECTURE.md:35` and `42` name `aio_fsync(O_DSYNC)` and `DirectorySync` through `sync_directory` | [artifact]; `completion-seam.md:17` is still wrong (N3) |
| F6 | yes | `io.h:274-275`: durable is the backend's promise, and plain on macOS survives a crash, not a power loss | [artifact] |
| F7 | yes | `PosixFile` is 16 bytes: a 2-byte `SyncCalls` (`posix_sync.h:147-188`), held by `static_assert`s (`posix_sync.h:188`, `posix_file.h:355`) | every build at b4600a8 (8fc497, R2, CI); P3 prints 16; the table in R1 and R3 |

### Tests at b4600a8: none dropped from the tree

`CMakeLists.txt` is unchanged since 113cee9. Without task and asio its suite is 22 tests (19
binaries, `completion_owner_braced_refused`, and the two Python checks). The slot's build-test ran
all 22 at b6f44b5 and at b4600a8. The 3 it lacked against 113cee9's 25 need
`COMPLETION_TASK`/`COMPLETION_ASIO`, which the slot's cache had switched off (N1), and R2 runs them.
The contributor's local count of 21 is not a job record and I did not reproduce it.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| N1 | no | gap | The "check build-test" at b4600a8 (and at b6f44b5) ran 22 of the suite's 25 tests. It reused the quest slot's `build` directory, whose CMake cache the f-mutant jobs had configured with `-DCOMPLETION_ASIO=OFF -DCOMPLETION_TASK=OFF`: configure took 3.4 s and built 38 targets, against 28.6 s and 46 targets at 113cee9. So `completion_seam`, `completion_asio_extension` (which runs SyncData, SyncFiles and OrderFiles on a full `PosixIO`) and `completion_asio_seam` did not run. Closed for this commit by R2 and CI. | 261124-111516-8fc497 against 261124-091927-b0b7d9; R2 261124-112648-36eb3f: 25 of 25 | The lead confirms the findings with R2 or CI as the suite job. The tool problem is that check jobs share a build cache with jobs configured differently; it is campaign-v3's to triage. |
| N2 | no | nit | `detail::sync_seams` is one table per image, not per process as `flush-mode.md:23`, `posix.h:154` and `CHANGELOG.md:34`/`43` say. Two shared libraries that compile completion each hold their own table, and each handed out index 1 for a different seam. An index is meaningful only to the image that interned it, so a test seam set through one image's inline `replace_sync_calls` on a backend whose syncs run in another image would reach the wrong entry. Production is unaffected: nothing interns there, and every entry no test interned is the system calls. The 255-seam limit is also per image. | R3 in 261124-113533-ed160a: "one table per image ... A's seam -> 1, B's seam -> 1"; "unused entry 200 in A: the default (system calls)" | The owner's call: say "per image" in those three places, or keep the text as is. |
| N3 | no | nit | Two mechanism docs this fix round left stale. `docs/mechanisms/completion-seam.md:17` says `PosixFile::sync_data` is `F_FULLFSYNC` on macOS (in plain mode it is `fsync`) and `fdatasync` on FreeBSD (it is `fsync`, and was at 02ab67f too; only AIO's native `SyncData` is `O_DSYNC`). `completion-seam.md:27` lists only the foreign-file fallback among `File::sync`'s callers, leaving out a weakened file's self-sync. `docs/mechanisms/sync-files.md:15` omits Linux's `fdatasync` from the seam. `sync-files.md:12` says another backend's file syncs itself, but another `PosixIO`'s file is batched unless the batch would weaken it. | the lines named; `posix_sync.h:75-92`; `posix.h:285-297` | Correct those sentences. |

Notes, none needing action in this quest:

- A full file in a plain batch costs one `F_FULLFSYNC` of its own, so N such files on one device
  cost N full flushes, about 20 ms each on this host (`flush-mode.md:5`), instead of one. This
  happens only in mixed-mode batches on macOS, and it is not measured.
- For Detent's migration: `replace_sync_calls` now also reaches `File::sync`, `File::sync_data` and
  the creation of `owner.lock` for files opened after it is called (`CHANGELOG.md:31-34`). A
  non-forwarding "skip" seam installed before files open now skips their own syncs, which the old
  code always issued for real. Detent's crash tests swap seams
  (`tests/detent_agent/test_agent_store_crash.cc:120-128`,
  `tests/detent_relay/test_relay_mirror_crash.cc:110-118`).
- I ran as a subagent of the lead's session, not as a session of my own titled
  `xapiand · reviewer completion-flush-mode durability`.

## Not run

- Power loss on hardware, and a faultfs crash-model run of either mode. faultfs keeps one mode per
  filesystem, so it cannot model a mixed-mode batch.
- Linux and FreeBSD locally. CI 45468242235 is the evidence for both. My probes are macOS-only
  (x86_64, APFS and devfs); macOS arm64 is CI's.
- Whether Linux's and FreeBSD's `fsync` flush the drive's cache on a given filesystem (for
  example FreeBSD UFS), as `flush-mode.md:5` says. This is a filesystem property the quest did
  not change.
- Filesystems other than APFS and devfs (HFS+, FAT, exFAT, SMB, NFS, FUSE).
- The cost of either mode, and of a weakened file's own flush.
- The external callers' migrations (journal-sync-mode, Detent, Xapiand, strata, faultlab).
- `replace_sync_calls` racing a sync on the same `PosixIO`. That is a data race on the backend's
  own member, the same as at 02ab67f, and the documented contract is to call it before opening the
  files it should see.
