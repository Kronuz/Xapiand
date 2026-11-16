# phase7c-l5: Deferred index I/O chores

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at strata:c82ac9f (2026-10-29); its ledger `docs/status/phase7c-l5.md` is the record. Its machine state is in `state.json`: `campaign status phase7c-l5`.

## Outcome

`defer_io` lets `apply` leave owed I/O chores for later owner-thread work instead of making device calls on each owed path. A WorkPool executor binding supports the posted merge and delete jobs.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] K1 `defer_io` prevents device calls on owed paths; faultfs call counts and controls verify it: `docs/status/phase7c-l5.md`
- A2 [artifact] K2 Owed verdicts and chores run, and retry is accepted at the same position: `docs/status/phase7c-l5.md`
- A3 [artifact] K3 Oversize, full-disk, failed-removal, and host-mode release cases follow the specified verdicts: `docs/status/phase7c-l5.md`
- A4 [artifact] K4 All 12 mutants are killed: `docs/status/phase7c-l5.md`
- A5 [artifact] K5 mac, Linux, FreeBSD preflights and the listed TSan suites pass: `docs/status/phase7c-l5.md`
- A6 [artifact] K6 `default_path_unchanged` matches main's verdicts, 163 mutating calls, file hash, and digest: `docs/status/phase7c-l5.md`
- A7 [artifact] K7 WorkPool binding passes the executor concept and background-merge suite: `docs/status/phase7c-l5.md`
- A8 [artifact] K8 `docs/mechanisms/defer-io.md` exists: `docs/status/phase7c-l5.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- `install()` remains inline because it reads and writes whole files; deferring it needs a new exception type.
- `pool_executor` is retired in favor of `work_pool_executor`.
- threadpool is pinned to e961e447.
- Risk high: Changes concurrency and executor interactions for index I/O (the lead's mapping).

## May land early

None.
