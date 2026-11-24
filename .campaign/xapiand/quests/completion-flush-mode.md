# completion-flush-mode: completion: PosixIO takes a required flush mode (plain or full) for every sync it issues

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status completion-flush-mode`.

## Outcome

`PosixIOOptions` carries a required flush mode, `plain` or `full`, chosen by whoever builds the
PosixIO, with no default and no process-global switch. It replaces `full_flush_ordering`: one
option, one mechanism. Every sync PosixIO issues follows it: `file_sync`, `PosixFile::sync` and
`sync_data` (the turn barrier's path), `files_sync_devices`, `sync_directory` and the lock-file
flush. On macOS, `full` is F_FULLFSYNC everywhere (today's durable syncs, plus what
`full_flush_ordering` did for ordering). `plain` is fsync for durable syncs and F_BARRIERFSYNC for
ordering barriers, which matches Xapiand master's WAL (Xapiand PLAN D177). On Linux and FreeBSD
both modes make the same calls as today.

## Acceptance criteria

- A1 Every sync call follows the mode: through the IO test seam, tests open a PosixIO in each mode
  and assert the exact calls by kind (file data, device flush, directory, lock file, turn-barrier
  `sync_data`), on macOS and on Linux. A mutant that keeps one call site at F_FULLFSYNC in `plain`
  mode, and one that issues fsync in `full` mode, are each killed by a named test
  (`--fails-without`).
- A2 `full` mode keeps today's refusals: an unsupported operation fails instead of falling back to
  a weaker call. The existing tests pass, and a mutant that degrades in `full` mode is killed.
- A3 Callers in this repository (tests, benches, examples) pass the mode explicitly. The journal
  lists the external callers (journal, Detent, Xapiand) and what each must pass. `full_flush_ordering`
  is gone, with no alias.
- A4 [artifact] `README.md` and `ARCHITECTURE.md` state per mode, per platform, which calls are
  made and what survives power loss on macOS ("plain fsync leaves data in the drive's cache").
  `CHANGELOG.md` names the API change.
- A5 The `asan` check, and CI green on every platform: [ci]

## Contracts

Provides `completion:flush-mode`, a breaking change to `PosixIOOptions`. All users are ours. The
journal migrates in `journal-sync-mode`; Detent and Xapiand migrate at their next completion
bump. Detent passes `full`.

## Decisions

- Owner, 2026-11-24 (Xapiand PLAN D177): the journal and its IO backend match Xapiand master's
  macOS defaults; `full` is the caller's explicit choice.
- Lead, 2026-11-24: completion owns every sync call (journal-sync-mode's contributor found the
  journal issues none itself), so the mode lives here. It replaces `full_flush_ordering` rather
  than sitting beside it.
- Detent's lead (msg-321, msg-323): the mode is explicit at construction with no global switch;
  Detent reviews durability through this quest's review questions.

## Review

One `durability` review, strong tier at effort max, read by Detent's lead for its acceptance:
1. Can any path reach a plain sync when the caller asked for full: defaults, reopen, the device
   flush, the directory barrier, the lock file, the turn barrier's direct `sync_data`?
2. Does the full-mode refusal path still fail and never degrade, shown by a run?
3. Are the per-platform claims in the docs true on Linux and FreeBSD, where both modes make the
   same call?

## May land early

Nothing: the option change and its call-site coverage land together.
