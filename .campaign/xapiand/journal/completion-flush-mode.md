# completion-flush-mode: journal

State: waiting on jobs 261124-091906-a47141 261124-091911-31b2b6 261124-091915-b8aea1 261124-091919-537673 261124-091923-20e986 261124-091927-b0b7d9
Quest: .campaign/xapiand/quests/completion-flush-mode.md (the lead's; never edit it)

## Doing

Ready for review at completion:113cee9 (quest/xapiand/completion-flush-mode, pushed); only jobs
remain: the five mutants and build-test, queued once at that commit. Local suite at 113cee9: 25 of
25 CTest tests pass (macOS x86_64); the five mutants were each killed locally before queueing.

## Next

1. Jobs finish: paste `campaign brief`'s table; on all green, State "ready for review".
2. Durability review (the lead launches it).

## Blocked on

Nothing. msg-326 (file set) answered by the lead: examples/**, bench/**, tools/** added.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | mutants, `--fails-without`, at completion:113cee9: f1 plain keeps F_FULLFSYNC for File::sync_data (261124-091906-a47141, completion_sync_calls_test), f2 plain keeps F_FULLFSYNC in sync_directory (261124-091911-31b2b6, completion_sync_calls_test), f3 full issues fsync (261124-091915-b8aea1, completion_sync_files_test), f4 plain keeps F_FULLFSYNC for File::sync, owner.lock and batch devices (261124-091919-537673, completion_sync_files_test); suite in build-test 261124-091927-b0b7d9 |
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
