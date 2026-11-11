# open-one-flush: The index open shares one device flush between its log and directory syncs

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status open-one-flush`.

## Outcome

`basic_index::open` makes the manifest log and its directory durable with one device flush
(`sync_files_and_directory`, one F_FULLFSYNC on macOS) instead of two, keeping both syncs'
guarantees (`docs/perf/open-sync.md`). Prototype: explore/open-sync, strata:54f9e0af.

## Acceptance criteria

Three to five, at outcome level: `- A1 <criterion>: <the test that shows it>`. An untagged one
needs a job tagged `--for A1` passing at the landed code, often the suite; tag one `[artifact]`
(a committed `path`), `[decision]` (a PLAN.md Decisions id) or `[manual]` (checked by hand) when no
job can show it.

- A1 Both crash properties hold: T4 case 9 and the rollover-stop cases in `test/index_test.cc`
  pass, and the mutant that syncs only on `rollover_owed_` fails case 9 (a job per side).
- A2 The open's tail improves on macOS: one strict job of at most 10 minutes (D59), interleaved
  pairs of `bench/open_sync/run.sh` at main and the quest head; p99 lower on most pairs.
- A3 The strata suite passes at the head (its suite check), with CI green on Linux.

## Contracts

None changed: the open's durability contract (manifest-log D6.6, C4) stays as is.

## Decisions

Owner, open: whether a read-only open may skip the syncs (needs a contract change; this quest
does not).

## May land early

None.
