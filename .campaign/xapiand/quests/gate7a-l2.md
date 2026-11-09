# gate7a-l2: Gate verification and safety fixes

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at Xapiand:addacf386 (2026-10-29); its ledger `docs/status/gate7a-l2.md` is the record. Its machine state is in `state.json`: `campaign status gate7a-l2`.

## Outcome

Gate checks cover T10 concurrency, sanitizer runs, fuzzing, coverage, and bulk profiling. The work fixes an Xapian stored-version term collision and terminates cyclic query conversion; one queryparser cycle fix remains assigned to the lead.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] L2: T10 under TSan; ctest under TSan and ASan+UBSan; docs E2E; one CPU-hour fuzzing per target; engine-code coverage reported: `docs/status/gate7a-l2.md`
- A2 [artifact] C1: `profile_bulk.sh` reports device-flush wait separately; T11 CPU row corrected: `docs/status/gate7a-l2.md`
- A3 [artifact] C2: mark `xapian_1` of hold 1 invalid; reject undersampled profiles: `docs/status/gate7a-l2.md`
- A4 [artifact] C4: trace version collision and fix `stored_version`: `docs/status/gate7a-l2.md`
- A5 [artifact] Add query-conversion termination test, fuzz watchdog, and replay: `docs/status/gate7a-l2.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Lead follow-up: queryparser must fix duplicate-node cycle and add a cycle check to its fuzz target.
- Sanitizer run uses local RapidJSON patch; stock run retains the known finding.
- Risk high: fixes query termination and version collision (the lead's mapping).

## May land early

None.
