# kv-scan: Add key-ordered index scanning

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at strata:89f9fd7 (2026-10-21); its ledger `docs/status/kv-scan.md` is the record. Its machine state is in `state.json`: `campaign status kv-scan`.

## Outcome

`basic_index::scan` performs a lazy k-way merge over the buffer's sorted view and segment key cursors. It exposes bounded and ranged scans while preserving snapshot semantics and newest-value precedence.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] Newest wins for a key in several sources; deletes hide older copies; updates and inserts between scans are seen: `docs/status/kv-scan.md`
- A2 [artifact] Empty index, from past last key, limits 0 and 1, and keys on block and restart boundaries: `docs/status/kv-scan.md`
- A3 [artifact] Explorer digest at seed 42 (`81bb0f0b7fe8d333`) is unchanged through the core API; ycsb_e CPU per op is within noise of 21.7 us: `docs/status/kv-scan.md`
- A4 [artifact] Full ctest, ASan+UBSan, a revert check per behaviour, and one mutant on the tie rule: `docs/status/kv-scan.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- API: `scan(from, n, f)` and `scan(from, to, n, f)`; n 0 means no limit; callback false stops; returns count.
- Snapshot rule: writer calls from callback throw `std::logic_error` and change nothing; gets and nested scans work.
- Buffer sorted view uses a main run and delta of at most 4,096 records; its bytes join `map_bytes()`.
- Risk high: Adds a shared public API and snapshot concurrency behavior (the lead's mapping).

## May land early

None.
