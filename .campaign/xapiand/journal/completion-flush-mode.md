# completion-flush-mode: journal

State: working
Quest: .campaign/xapiand/quests/completion-flush-mode.md (the lead's; never edit it)

## Doing

Round 1 code written in the completion worktree
(`/Users/kronuz/code/Kronuz/completion-wt/xapiand/completion-flush-mode`, branch
quest/xapiand/completion-flush-mode): include/, test/, docs. Building the test targets locally
(`~/.scratch/xapiand/completion-flush-mode/build`, -G Ninja) under `quiet light`.

## Next

1. Build and run the sync tests (sync_primitives, sync_calls, sync_files, sync_seam, sync_data) and
   the rest of the suite's test targets; fix.
2. When the lead widens the file set (msg-326): examples/demo.cc, bench/*_probe.cc pass the mode;
   tools/mutants.py anchors m1/m9 follow `file_sync(owner->fd(), sync_calls_)`, m9 now expected
   killed.
3. Mutant commits (each its own commit off the branch head) and `--fails-without` jobs:
   A1-plain (file_sync keeps F_FULLFSYNC in plain), A1-plain-dir (sync_directory flushes in
   plain), A1-full (file_sync issues fsync in full), A2 (full falls back to fsync on ENOTSUP).
4. `campaign verify completion-flush-mode`; push; ready for review.

## Blocked on

Lead: the file set (msg-326 to the lead). `examples/demo.cc` is built at top level and constructs
`PosixIO(argv[1])`; five `bench/*_probe.cc` construct PosixIO; `tools/mutants.py --check` is a CTest
test whose m1 and m9 anchors contain `detail::file_sync(owner->fd())`, which this change rewrites.
Recommend adding `examples/**,bench/**,tools/**` to the file set. Everything else proceeds.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | <pending> |
| A2 | <pending> |
| A3 | <pending> |
| A4 | <pending> |
| A5 | ci:linux-asan-ubsan |

## Result

<pending>

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
- Tests that do not test sync semantics build their PosixIO in `full`, the calls they made before
  for durable syncs.
