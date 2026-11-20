# name-index review: durability

Commit: xapiand:1f78c77321f939224ece4aa1afb24a8fd8a4b551
Verdict: APPROVE
Date: 2026-11-17

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- The accepted correctness review (`name-index-correctness.md`): `name_index.cc` writes nothing, and its F2 fix
  (a build fails on an `opendir` error other than ENOENT or ENOTDIR) is in this commit (xapiand:74faa0d).
- A5's test, `test_a_kill_during_create_leaves_the_names_consistent` (`tests/test_node.cc`): a forked child creates
  nested and sharded indexes through the pool and is SIGKILLed in three rounds; the restarted names equal the walk,
  and every create the child reported done is in the walk. Its last green run is 261117-124524-bb4872 at
  xapiand:e422166; the code since then adds an `#include <mutex>` and merges master (none of it in the names' crash
  path). The rerun at this commit is the lead's (`campaign brief name-index`).

## What I checked myself

Read, no probe was needed for these:

1. Nothing on disk depends on the names (Q4). `src/database/name_index.cc` has no file I/O: `build()` reads through
   `index_pattern::expand` and keeps the result in memory (`name_index.cc:236-274`); `opened()` only inserts. The
   only change to a write path is one line in `make_host`'s resolve (`src/database/pool.cc:570`), after
   `fs::create_directories(dir)` and before the journal opens, so it neither adds nor reorders a disk write. A kill
   during a build loses an in-memory set and a claim; the next start begins with `complete_ == false` and walks.
2. A stale name cannot reach the disk. A search opens the names it gets with `DB_OPEN` or `DB_OPEN | DB_WRITABLE`
   (`src/server/search_views.cc:1981-1985`), never `DB_CREATE_OR_OPEN`, so resolve runs with `op.create == false`
   and throws `index_not_created` for a missing directory (`pool.cc:563-566`). A listed name never recreates a
   directory.
3. An acknowledged create is never missing from the names after a restart (Q5), because the names after a restart
   are the walk of the disk: patterns walk until the build ends (`search_views.cc:2356-2363`), and the build's base
   is that walk. Whatever the disk lost, the names lost with it, and nothing more.
4. The order inside a create: `opened()` runs once `path/strata` exists, which is exactly when the walk starts
   listing the index (D2), so the window between them is empty in both directions. A create that fails after
   `create_directories` leaves an empty `strata` that both list (D8).

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| U1 | no | gap | Pre-existing, outside this quest: on power loss (not a process kill), an acknowledged create can lose its index directory, because resolve makes `path/strata` with `fs::create_directories` and fsyncs no parent, while `PosixIO` requires that "the application establishes and durably publishes this directory first". The names agree with the disk either way (Q5), so this is not the name index's, but the index itself is gone. | `src/database/pool.cc:568`; `completion/include/completion/posix.h:613`; unchanged from master. | For x-pool's owner or a durability hunt: fsync each created ancestor's parent, from the data directory down, before the first journal write. Test: an injected `fsync` hook that records the directories synced on a fresh create. |
| U2 | no | gap | A5 models a process kill, which keeps the page cache. No test models a lost directory entry (power loss); with U1 that is the case where the disk, and so the names, lose an acknowledged index. | `tests/test_node.cc` A5. | With U1's fix, not before. |

## Not run

- No power-loss or torn-metadata model (a VM pulled mid-create); U1 is read from code.
- No rerun of A5 at this commit; its last green run is at xapiand:e422166 (see above).
- Full disk and EIO during the build are covered by the correctness review's F2 fix and test, not rechecked here.
- No Linux or FreeBSD run.
