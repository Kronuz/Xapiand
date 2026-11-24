# completion-flush-mode: journal

State: ready for review (fix round 1 at completion:b6f44b5, pushed); waiting on jobs 261124-110320-b7760e 261124-110324-f3f38e 261124-110328-fbd236 261124-110332-cae4a1 261124-110339-12e18b 261124-110342-c05f38 261124-110423-819ee6 (build-test)
Quest: .campaign/xapiand/quests/completion-flush-mode.md (the lead's; never edit it)

## Doing

Fix round 1 (durability review, CHANGES, F1-F7) at completion:b6f44b5. Local, under quiet light
(macOS x86_64): sync_primitives, sync_calls and sync_files pass; f6-f9 each killed locally; the full
build (all targets) compiles. Evidence queued once at b6f44b5.

## Next

1. Jobs finish: paste `campaign brief`'s table; then the lead confirms the findings
   (`campaign accept ... --confirm F1=... F2=...`, the suite job).

## Blocked on

Nothing.

## Fix round 1: findings

| Finding | Fix (completion:39c1394 code+tests, 77356f1 docs, b6f44b5 mutants) | Test | Mutant (`--fails-without`) |
|---|---|---|---|
| F1 | `PosixIO::batch_fds`: a `PosixFile` whose mode the batch would weaken (`batch_weakens`: macOS, full file in a plain batch) syncs itself through `file->sync()` first, as a foreign file does; one helper for `sync_files` and `sync_staged` (both batches) | `sync_primitives_test` `cross_backend` | f6 (the old loop: every PosixFile batched, as at 113cee9) 261124-110320-b7760e |
| F2 | none needed (code correct) | `sync_primitives_test` `reusable`, per mode: `make_closed_file` + `create_exclusive_into`, `open_existing_into`, `open_reclaim_candidate_into`, each `File::sync` and `sync_data` recorded | f7 (= reviewer's g6) 261124-110324-f3f38e; g6 also in 261124-110342-c05f38 |
| F3 | `FlushMode::Value { plain = 1, full = 2 }`, no zero; `FlushMode(Value)` throws `invalid_argument` for any other value (constexpr: a constant one fails to compile); `PosixIO` refuses options whose mode bytes are neither | `sync_primitives_test` `flush_mode_values` (0, 3, 255; value-initialized; memset options) | f8 261124-110328-fbd236, f9 261124-110332-cae4a1 |
| F4 | README table and CHANGELOG state plain's `F_FULLFSYNC` fallback and its failure | [artifact] completion `README.md`, `CHANGELOG.md` | - |
| F5 | ARCHITECTURE: FreeBSD AIO syncs `Sync` (`aio_fsync(O_SYNC)`) and `SyncData` (`aio_fsync(O_DSYNC)`) natively, `DirectorySync` through `sync_directory`; the `sync_data` FreeBSD cell names `O_DSYNC` | [artifact] completion `ARCHITECTURE.md` | - |
| F6 | `io.h` above `sync_files`: durable is the backend's promise; plain on macOS survives a crash, not a power loss | [artifact] completion `include/completion/io.h` | - |
| F7 | `SyncCalls` = mode byte + one-byte index into `detail::sync_seams`, a process-wide table of interned seams (entries written once under a mutex, never changed); `static_assert(sizeof(SyncCalls) == 2)` and `static_assert(sizeof(PosixFile) == sizeof(void*) + 2 * sizeof(int))` | sizeof(PosixFile) on 64-bit: 16 (02ab67f) -> 48 (113cee9) -> 16 (b6f44b5), held by the static_asserts in every build | - |

f1-f5 not rerun: the lines they replace (`sync_call`'s rows, `issue`'s `full_fsync` case) are
byte-identical at b6f44b5, and the tests that killed them are unchanged (only added to).

### F1's class: every path where a file is synced by something other than its own backend

| Path | Before (113cee9) | Now | Value-checking test |
|---|---|---|---|
| `sync_files` (macOS `files_sync_devices`; Linux/other `files_sync`) | batching backend's mode | `batch_fds`: weakened file syncs itself | `cross_backend` final `sync_files` row, both directions |
| `sync_files_and_directory` (macOS `sync_staged` durable; else `sync_files` + `sync_directory`) | batching mode | same helper | `cross_backend` SyncFiles via IO method |
| `order_files_and_directory` (macOS `sync_staged` ordered) | batching mode | same helper | `cross_backend` OrderFiles via IO method |
| `Sync` primitive (`io.sync_files`, operation.h:159) | batching mode | same helper | `cross_backend` Sync via drive_synchronously, native queue (macOS) |
| `SyncFiles`, `OrderFiles` primitives | batching mode | same helper | `cross_backend` via drive_synchronously and native queue |
| `SyncData` primitive / `sync_data_files` | file's own (`File::sync_data`) | unchanged | `cross_backend` SyncData rows |
| Reuse: `make_closed_file` then `*_into` | backend's mode at make; another backend's `*_into` refused | unchanged | `reusable` (F2); refusal: `reuse_test.cc:212-216`, `create_into_test.cc:146` |
| Reopen: `open_existing`, `open_reclaim_candidate`, read-only | the opener's mode (a new file) | unchanged | `call_sites`, `sequences` |
| Moves (`unique_ptr`, `shared_ptr` of a File) | the object moves; no copy of a PosixFile exists | unchanged | n/a (PosixFile is not copyable: File's copy is deleted) |
| Linux io_uring `Sync`/`SyncData`, FreeBSD AIO `Sync`/`SyncData` | kernel call, mode not read | unchanged; equal in both modes there | `table_rows` (Linux/other rows identical per mode, `batch_weakens` false off macOS) |

### Probes

The reviewer's probe scripts build against completion's `detail::` internals that F3 and F7
change (P2 sets `SyncCalls::fsync_call`, P3 builds `FlushMode` from 0 expecting no throw, g5's
replacement sets `fsync_call`). The jobs run a copy at
`.scratch/xapiand/completion-flush-mode/probe/` (home) with three edits, nothing else:
P2 sets `calls_for_mode.seam = intern_sync_seam({&rec_fsync, &rec_fcntl})`; P3 catches and prints
the refusal; g5's replacement copies `sync_calls_` and sets `mode = plain`. Jobs:
261124-110339-12e18b (P1-P5, P7), 261124-110342-c05f38 (P6, g1-g6).

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | fix round 1 at b6f44b5: f6-f9 (table above), build-test 261124-110423-819ee6; round 0: mutants, `--fails-without`, at completion:113cee9: f1 plain keeps F_FULLFSYNC for File::sync_data (261124-091906-a47141, completion_sync_calls_test), f2 plain keeps F_FULLFSYNC in sync_directory (261124-091911-31b2b6, completion_sync_calls_test), f3 full issues fsync (261124-091915-b8aea1, completion_sync_files_test), f4 plain keeps F_FULLFSYNC for File::sync, owner.lock and batch devices (261124-091919-537673, completion_sync_files_test); suite in build-test 261124-091927-b0b7d9 |
| A2 | mutant f5, full falls back to fsync on ENOTSUP/EOPNOTSUPP/ENOTTY (261124-091923-20e986, completion_sync_calls_test `full_refusal`); existing tests in build-test 261124-091927-b0b7d9 |
| A3 | 261124-091935-9baf66 (no `full_flush_ordering` in include, test, bench, examples, tools); every in-repo caller passes the mode (the build in 261124-091927-b0b7d9 compiles them; `PosixIO(dir)` does not compile); external callers below |
| A4 | [artifact] completion `README.md` "Flush modes", `ARCHITECTURE.md` "Flush modes" (per site, per platform, per mode; power loss on macOS), `CHANGELOG.md` 2026-11-24, `docs/mechanisms/flush-mode.md` |
| A5 | `ci:linux-asan-ubsan` (CI on the pushed branch) |

Examined and cancelled before running (superseded by msg-1's rework): 261124-085425-5cc379,
261124-085430-2dfd66, 261124-085434-a07900, 261124-085439-fbbac4, 261124-085443-8610fd,
261124-085458-af1e5e, 261124-085527-f471a5. 261124-085506-4e19d2 (A3 at cc82500) passed.

## Result

completion:113cee9 on quest/xapiand/completion-flush-mode. `PosixIOOptions::flush_mode`
(`FlushMode::plain` | `FlushMode::full`) is required (no default constructor: `PosixIOOptions{}`
and `PosixIO(dir)` do not compile) and replaces `full_flush_ordering`. One table,
`sync_call(SyncSite, FlushMode, SyncPlatform)` in `include/completion/posix_sync.h`, names the
call at every site; `file_sync` (File::sync, File::sync_data, owner.lock), `files_sync_devices`
and `sync_directory` issue only it. macOS full: F_FULLFSYNC everywhere; plain: fsync, and one
F_BARRIERFSYNC per ordering point; Linux and FreeBSD identical in both modes. Full never degrades.
Each PosixFile copies its backend's SyncCalls (mode + seam); the Linux seam gains fdatasync.
How it works: completion `docs/mechanisms/flush-mode.md`; tables in README and ARCHITECTURE.
Not measured: either mode's commit cost (bench/commit_probe.cc now runs both modes).

External callers to migrate (signature: `PosixIO(dir, options, artifact_mode = 0600)`,
`make_posix_io(ctx, dir, options, mode)`; options `{.flush_mode = ...}` or positional
`{FlushMode::X, follow, shared}`; `make_shared`/`make_unique` need `PosixIOOptions{...}` spelled):
- journal (journal-sync-mode): bench/{bench,checkpoint_bench,completion_bench,idle_cost,reclaim_bench,turn_bench}.cc,
  examples/demo.cc, test/{completion,crc_fixture,host,inventory}_test.cc construct `PosixIO(dir)`;
  mode per that quest (plain per D177 unless a test needs full).
- Detent: pass `full` everywhere: src/detent_authority/service.cc (4 sites), src/detent_local/disk_store.cc
  (`(dir, options.artifact_mode)` -> `(dir, {.flush_mode = full}, mode)`), src/detent_relay/mirror_service.cc,
  benchmarks/{detent_authority_durable,relay_window}_bench.cc, tests/detent_agent/test_agent_store*.cc;
  Linux `replace_sync_calls(fsync, writeback)` -> `(fsync, fdatasync_or_nullptr, writeback)`.
- Xapiand: src/engine/strata/io_options.h (`PosixIOOptions{true, true}` + `full_flush_ordering` ->
  `{opts mode, true, true}`), src/opts.{h,cc} `--full-flush-ordering` becomes the mode switch
  (plain default, D177), src/database/pool.cc and src/engine/strata/shard.cc drop the `0600`,
  tests/test_io_options.cc, tests/test_reactor_requests.cc.
- strata: bench/* and test/directory_test.cc, test/counted_io_test.cc (`PosixIOOptions{true, true}`,
  `(dir, 0600, opts)`, `PosixIO(dir)`).
- faultlab: test/recorder_test.cc (`make_shared<j::PosixIO>(dir)`); may adopt `sync_call` for `call_for` (msg-1).

## Messages

- msg-1 (lead, faultlab's call table): done. `kronuz::journal::sync_call(SyncSite, FlushMode,
  SyncPlatform = host_sync_platform)` in completion `include/completion/posix_sync.h` is the one
  table from a site (`file`, `data`, `directory`, `order`) and mode to the call (`fsync`,
  `fdatasync`, `full_fsync`, `barrier_fsync`), with the same platforms as faultlab's
  (`macos`, `linux_kernel`, `other`). `file_sync`, `files_sync_devices` and
  `PosixIO::sync_directory` issue only what it names; `table_rows` in
  `test/sync_primitives_test.cc` checks every row. faultlab can replace `call_for` with it.
- msg-326 (mine, to the lead, file set): answered; examples/**, bench/**, tools/** added.

## Cancelled jobs

- 261124-085425-5cc379, -085430-2dfd66, -085434-a07900, -085439-fbbac4, -085443-8610fd (mutants
  f1-f5), 261124-085458-af1e5e and -085527-f471a5 (build-test): queued before msg-1 changed the code
  they target; cancelled before running. I queue once, at the ready commit, from now on.

## Rulings I made

- `FlushMode` is a class over an unscoped `Value { plain, full }` with no default constructor, so a
  missing mode is a compile error (`PosixIOOptions{}`, `PosixIO(dir)`), not a runtime check.
  `flush_mode` is the first field of `PosixIOOptions`.
- The constructors and `make_posix_io` take `options` before `artifact_mode` (which keeps its 0600
  default): a required parameter cannot follow a defaulted one, and the other order would force
  `0600` at ~80 call sites.
- The mode lives in `detail::SyncCalls` beside the seam's call pointers; each `PosixFile` copies
  its backend's `SyncCalls` at open (files may outlive the backend). So `replace_sync_calls` sees
  the files opened after it; its doc says so.
- Linux's seam gains `fdatasync` (`replace_sync_calls(fsync, fdatasync, writeback)`) so A1 can
  assert `sync_data` there. Detent's tests call the two-argument Linux form and migrate.
- macOS plain, ordering point: barrier on the directory's device; files on other devices keep
  their fsync (no full flush), because plain promises nothing against power loss.
- macOS plain, unsupported barrier (ENOTSUP/EOPNOTSUPP/ENOTTY): still falls back to F_FULLFSYNC,
  the existing fail-closed rule (stronger, never weaker). Not counted as "plain issues
  F_FULLFSYNC".
- Plain durable batches skip the per-device fstat discovery: there is no device flush to place.
- Tests that do not test sync semantics, the probes and the demo build their PosixIO in `full`,
  the calls they made before for durable syncs. commit_probe runs its patterns per mode (the old
  "temporary ordered" and mixed "one ordering point" shapes no longer exist).
- `order_files_and_directory` is always the ordered batch; the table makes its ordering point a
  full flush in full mode (same calls as the durable batch, checked by `full_protocol`).
- The mutants are rows f1-f5 of completion `tools/mutants.py`; m9 (owner.lock sync) now expects
  killed. Mutant commits: refs/mutants/completion-flush-mode/f1..f5 in the completion repo.
- Fix round 1, F1: "stronger" is `batch_weakens(file, batch)`: the file is full and the host's
  `sync_call(file)` differs between the modes (macOS only), so Linux and FreeBSD batches make
  exactly today's calls. A plain file in a full batch stays batched (gets the stronger flush).
- Fix round 1, F3: refusal at construction (constexpr throw; runtime `invalid_argument`) rather than
  a compile error, which a runtime `Value` cannot get; `sync_call` keeps `mode == full` (g2/f3
  anchors) since no invalid `FlushMode` is constructible and `PosixIO` checks raw bytes.
- Fix round 1, F7: the seam is a one-byte index into a process-wide interned table (the lead's
  "static"), not a per-file pointer: 16 bytes instead of 24, and files outliving their backend
  stay valid because entries are never freed. 255 distinct seams per process; one more throws
  `length_error`, so `replace_sync_calls` is no longer `noexcept`. Its lifecycle table is in
  completion `docs/mechanisms/flush-mode.md` ("The seam table"); I judged it a data layout the
  lead's ruling prescribed, not a new state machine needing a design check first.
- Fix round 1: the F1-F3 tests live in `sync_primitives_test`, which has every route (IO method,
  drive_synchronously, native queue) and per-platform logs; reviewer's run_mutants runs it.
