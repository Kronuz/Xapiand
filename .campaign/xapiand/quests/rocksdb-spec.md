# rocksdb-spec: RocksDB fairness audit and mechanism spec

Risk: low. History: this landed before the campaign, under the earlier campaign skill, at strata:d4fec9a (2026-10-20); its ledger `docs/status/rocksdb-spec.md` is the record. Its machine state is in `state.json`: `campaign status rocksdb-spec`.

## Outcome

The harness fixes account for unequal workloads and merge behavior. A prose-and-diagrams specification maps RocksDB mechanisms to strata's measured losses.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] A1-A6 audit findings recorded with raw data: `docs/status/rocksdb-spec.md`
- A2 [artifact] Harness corrected where the comparison was unequal: `docs/status/rocksdb-spec.md`
- A3 [artifact] RocksDB mechanisms, invariants, costs, and measured strata losses documented: `docs/status/rocksdb-spec.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- None recorded in the ledger.
- Risk low: research, measurement, and documentation (the lead's mapping).

## May land early

None.
