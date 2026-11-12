# sync-at-first-commit: An open runs no sync; the first commit syncs what it inherited

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status sync-at-first-commit`.

## Outcome

An open of a strata index takes the exclusive directory lock as today and runs no fsync or F_FULLFSYNC (D60c): `basic_index::open` drops its `sync_files_and_directory({&log, 1})` (include/strata/core/index.hh:5265, from open-one-flush). The first commit on the handle makes what it inherited durable (the log's unsynced tail edit and a rollover's unsynced rename, design cases 9 and 10 of open-one-flush) before it acknowledges anything, folded into its own commit's sync where the order allows. The accepted losses are stated in the design note.

## Acceptance criteria

Three to five, at outcome level: `- A1 <criterion>: <the test that shows it>`. An untagged one
needs a job tagged `--for A1` passing at the landed code, often the suite; tag one `[artifact]`
(a committed `path`), `[decision]` (a PLAN.md Decisions id) or `[manual]` (checked by hand) when no
job can show it.

- A1 [artifact] A design note fixes what an open may observe, what the first commit syncs and in what order against its own writes, each crash point under the crash model, and D60c's accepted losses (a search may see an unacknowledged commit that power loss can erase; EIO at first commit; the first write pays): `docs/design/sync-at-first-commit.md`
- A2 An open runs no fsync or F_FULLFSYNC, shown by a test counting syncs through strata's file hooks: the suite, with a `--fails-without` job (the open's sync kept)
- A3 Every acknowledged commit survives a crash at each step of the first commit after an open, including an inherited unsynced tail edit and an unsynced rollover rename, under faultfs: tests in the suite, with `--fails-without` jobs (inherited sync dropped; inherited sync after the acknowledgement)
- A4 Open latency under the loaded harness (bench/open_sync/loaded.sh) before and after, and the first commit's latency after an open before and after: one exclusive A/B job of at most 10 minutes each (D59), honest magnitudes

## Contracts

Changes strata's open contract: an open does not make inherited state durable; the first commit does, before its acknowledgement. No API change. Xapiand picks it up through xapiand-strata-bump or the next pin.

## Decisions

- Owner, 11-12 (D60c): no read-only mode; the sync is a writer's duty.
- Lead: read-only-open's branch (quest/xapiand/read-only-open, strata:6f83a4ff) holds the sync-counting tests, faultfs hooks and the open_sync/idle_index harness; reuse them, not its shared-lock code.

## May land early

The design note (A1).
