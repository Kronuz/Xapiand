# drop-dir-reader: journal

State: ready for review
Quest: .campaign/xapiand/quests/drop-dir-reader.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Done; completion:3f82cda pushed, CI running.

## Next

1. Lead: confirm CI green on 3f82cda (A2).

## Blocked on

Nothing

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | job 261112-075935-faac3d (build-test at completion:3f82cda, exit 0; includes completion_mutants_anchors) |
| A2 | 261112-075935-faac3d; ci:ci / linux-x86_64-gcc, ci:ci / macos-arm64, ci:ci / freebsd-x86_64 (CI run green on every job at completion:3f82cda) |

## Result

completion:3f82cda removes acquire_directory_reader (io.h, posix.h), its owner_lock_test cases, m15 (24 mutants again in tools/mutants.py and mutants.yml) and docs/mechanisms/directory-locks.md. The tree matches completion:dfdac5d (before the shared lock) exactly. No user in strata, prospect or Xapiand checkouts (grep: only campaign files and a review scratch).

## Rulings I made

- lock(fd, kind, ...) folded back to lock_exclusive: with one kind left the kind parameter is dead.
- directory-locks.md deleted, not reduced: nothing links to it and no exclusive-only content existed before the shared lock.
- docs/reviews/cl-2-code.md left alone (review history).
