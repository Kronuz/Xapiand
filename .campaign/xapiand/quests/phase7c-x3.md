# phase7c-x3: Coroutine views and streamed bodies

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at Xapiand:d5a2e746c (2026-11-01); its ledger `docs/status/phase7c-x3.md` is the record. Its machine state is in `state.json`: `campaign status phase7c-x3`.

## Outcome

Searches and HTTP handling run as coroutines on reactors with request budgets and view admission. RESTORE parses ahead in chores, and large stored uploads stream to storage.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] 1. Pins and service: coroutine SearchService, demoted cap, cancellation, and service setup: `docs/status/phase7c-x3.md`
- A2 [artifact] 2. Views on the reactor: budgeted searches, per-index admission, and debug wait checks: `docs/status/phase7c-x3.md`
- A3 [artifact] 3. Bodies and blocking paths: streamed body reader, RESTORE parse-ahead, task bulk indexer, stored uploads, and FLUSH: `docs/status/phase7c-x3.md`
- A4 [artifact] 4. Proof: builds, tests, E2E, sanitizers, streaming, restore, and crash checks: `docs/status/phase7c-x3.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Use frame-local search_task rather than search_pinned because scorer captures point into the snapshot.
- Streamed stored uploads choose the shard from the document id; large blob read-side streaming remains open.
- Risk high: concurrency, cancellation, and storage writes (the lead's mapping).

## May land early

None.
