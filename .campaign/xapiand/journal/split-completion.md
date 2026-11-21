# split-completion: journal

State: waiting on jobs 261121-130537-3bead5 (A1 build-test) 261121-130537-2f2f5d (split check) 261121-130550-2aed7e (A3) 261121-130554-a61cdb (A6)
Quest: .campaign/xapiand/quests/split-completion.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Split done at completion:2a13cc671db4 (pushed to origin/quest/xapiand/split-completion); jobs queued.

## Next

1. When the jobs finish: fill the Evidence table from `campaign brief`, set State "ready for review".

## Blocked on

Nothing.

## Evidence

Open branches checked before cutting: `cl-2-detent-replay` and `origin/phase/sync-files` edit
posix.h but are stale (their commits landed on main in other form: CL-2 and "Make the read-only
open virtuals pure" are in main's log). Detent's completion quests (completion-faultfs dropped;
completion-sync-data, drivers-completion-seam, completion-sync-files, completion-task-driver
landed): none open. No other quest/* branch touches posix.h.

| Criterion | Evidence |
|---|---|
| A1 | build-test 261121-130537-3bead5 (queued) |
| A2 | split_check (split-check-sources worktree version) 261121-130547-c392d9: exit 0, 0 errors, 0 warnings; the landed-tool `split` check 261121-130537-2f2f5d (queued) |
| A3 | 261121-130550-2aed7e: one job, ctest -N at base 714186eab63d ($JOB_REF_BASE) and at head, `Test #` lines diffed (queued) |
| A4 | docs/source-layout.md at completion:2a13cc671db4 |
| A5 | CI on quest/xapiand/split-completion (pushed) |
| A6 | selfcontained.sh over posix_support.h, posix_sync.h, posix_volume.h, posix_file.h (every old header `-x`'d): 261121-130554-a61cdb (queued) |
| A7 | lead's reading |

split_check warnings to answer: none (0 warnings).

## Result

`include/completion/posix.h` (1,212 lines) now holds `PosixIOOptions`, `PosixIO` and
`make_posix_io` (504 lines) and includes four pieces cut at the file's own boundaries:
`posix_support.h` (122, shared detail helpers), `posix_sync.h` (171, sync calls and
`files_sync_devices`), `posix_volume.h` (250, filesystem class and the macOS barrier-storage
probe), `posix_file.h` (351, `PosixFile`, `PosixCursor`). Layout note: docs/source-layout.md.
No file in completion is over 1,000 lines now. Outside this quest's file set, left for the lead:
AGENTS.md's repo map line for posix.h, and the "where it lives" lines in
docs/mechanisms/barriers.md and docs/mechanisms/commit-barriers.md, which name posix.h (the
latter with line numbers already stale on main).

## Rulings I made

- Split rather than keep whole: posix.h held four units a maintainer works on apart from PosixIO
  (the sync-call machinery with its own test, filesystem classification, the dlopen'd IOKit
  probe, the file/cursor classes). PosixIO itself is one class and stays whole in posix.h.
- `posix_support.h` holds 73 moved lines, under the ~100-line join threshold: it is the shared
  detail header SPLITTING.md prescribes (`system_failure` and `inspect_status` are used by
  sync, file, PosixIO and bsd_completion.h); joining it to posix_sync.h would make the file
  piece depend on the sync piece for unrelated helpers.
- Flat `posix_*.h` names, not a `posix/` directory: the repository's headers are flat
  (`bsd_completion.h`, `linux_completion.h`).
- The platform include blocks moved with their users: the `statfs` block (and its
  `KRONUZ_COMPLETION_STATFS` macros) and `<dlfcn.h>` to posix_volume.h, `<sys/acl.h>` /
  `<sys/xattr.h>` to posix_file.h. posix.h gets the macros through posix_volume.h; it keeps its
  other original includes unchanged.
- No namespace-scope object with a dynamic initializer moved (the function-local statics in
  `next_posix_instance`, `AppleRegistry::get` and `barrier_volumes` move with their functions).
  Header-only library, one TU's text reassembled by the umbrella, so inlining is unchanged.
- Nothing compiled locally; split_check.py (static, no build) run once before submitting.
