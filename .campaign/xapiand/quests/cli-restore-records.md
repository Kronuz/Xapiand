# cli-restore-records: CLI --restore into an empty data directory writes the index's shard records

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status cli-restore-records`.

## Outcome

`xapiand --restore INDEX --in FILE` into a new, empty data directory leaves an index a server then
serves in full: the same documents as a RESTORE over HTTP, any shard count. Today the CLI path leaves
`.xapiand/indexes` without the `rt/.__N` shard records, so a served COUNT fails with "Inconsistency in
number of replicas configured" (default shards) or returns 0 (`--shards 1`) (docs-upgrade-page,
reproduction job 261109-174137-c5db06). It is the upgrade page's documented path for alpha.9 and
alpha.10 data. The refusal text at `src/main.cc:666` also names alpha.10 data, which is Xapian too
(hunt row).

## Acceptance criteria

- A1 A CLI restore into an empty directory, then serving it, answers COUNT and DUMP with exactly the restored documents, for the default shard count and for `--shards 1` and `--shards 3`: `tests/restore_cli_check.sh` run as a job, and the same check fails at main's code (the job at main, or the fix reverted)
- A2 The cause is fixed where the records are written, not by a second code path for the CLI: the journal names the function that writes shard records on the HTTP path and shows the CLI path now goes through it; a mutant removing the fix is killed by A1's check or a unit test
- A3 A CLI restore into an index that already exists (created by `PUT`) still restores its documents (reproduction case 4 stays green): in A1's check
- A4 The refusal message (`src/main.cc` and `src/xapian_data_guard.h`) names "alpha.10 or earlier" (or every Xapian-based release): [artifact] a quote in the journal row and the `build-test` check. The changelog entry is written by docs-upgrade-page, which owns `docs/src/content/docs/changelog.md` and lands after this quest (lead, 11-09).
- A5 The `build-test` check passes, and CI is green

## Contracts

None changed: the CLI flags and the HTTP API stay as they are.

## Decisions

- Lead, 11-09: a defect fix, not a narrowing; the page keeps documenting the CLI path.

## May land early

Nothing; one landing.
