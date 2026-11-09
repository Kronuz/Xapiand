# core-vs-rocksdb: Quick-tier key-value RocksDB comparison

Risk: low. History: this landed before the campaign, under the earlier campaign skill, at strata:b8a2cb6 (2026-10-20); its ledger `docs/status/core-vs-rocksdb.md` is the record. Its machine state is in `state.json`: `campaign status core-vs-rocksdb`.

## Outcome

The benchmark measures strata's search layout as a key-value store against RocksDB. Results and raw data document the quick-tier comparison and profile its top three losses.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] Harness and benchmark plan completed: `docs/status/core-vs-rocksdb.md`
- A2 [artifact] Fallback to search_layout added because basic_index<document_layout> does not compile on main: `docs/status/core-vs-rocksdb.md`
- A3 [artifact] Overlapping rows rerun and labeled: `docs/status/core-vs-rocksdb.md`
- A4 [artifact] Results, raw data, perf index row, and ledger recorded: `docs/status/core-vs-rocksdb.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Owner and lead relabeled the work as "strata's search layout used as a key-value store," not a core comparison.
- Lead asked the work to stop after the quick-tier comparison.
- Risk low: measurement and performance research (the lead's mapping).

## May land early

None.
