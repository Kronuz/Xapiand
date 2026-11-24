# journal-sync-mode: journal

State: working
Quest: .campaign/xapiand/quests/journal-sync-mode.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

<The current step, precisely enough for a fresh contributor to continue it.>

## Next

1. <step>

## Blocked on

<What, and who decides. "Nothing" when nothing.>

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), name every job you examined, failures included (`campaign brief` lists failed
jobs no journal names), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| <A1> | <from `campaign brief`> |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

<Decisions inside the quest's scope, so the reviewer can check them.>

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

| Criterion | Evidence |
|---|---|
| A1 | <no evidence yet> |
| A2 | <no evidence yet> |
| A3 | <no evidence yet> |
| A4 | <no evidence yet> |
| A5 | <no evidence yet> |

### Result

None yet.

### Rulings I made

None.
