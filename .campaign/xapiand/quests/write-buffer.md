# write-buffer: Single-writer buffered writes

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at strata:39c9cc1 (2026-10-26); its ledger `docs/status/write-buffer.md` is the record. Its machine state is in `state.json`: `campaign status write-buffer`.

## Outcome

The index buffers records in chunked lanes with key and docid maps, then freezes and flushes buffers into segments. A lease coordinates writers and chores; readers beyond the lease holder require views.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] K1 One-writer items 2 and 3 build the behavior specified by D2-D4, D6, D8, and D10: `docs/status/write-buffer.md`
- A2 [artifact] K2 ctest passes unchanged apart from shims; sanitizer counts and byte equivalence are recorded: `docs/status/write-buffer.md`
- A3 [artifact] K3 New unit tests cover each decision; each WB finding test kills its fix mutant: `docs/status/write-buffer.md`
- A4 [artifact] K4 Mutants cover every new ordering or ownership check and are killed by named tests: `docs/status/write-buffer.md`
- A5 [artifact] K5 ASan+UBSan and TSan ctest pass on the rebased head: `docs/status/write-buffer.md`
- A6 [artifact] K6 Memory per buffered record and per open index is measured against the base: `docs/status/write-buffer.md`
- A7 [artifact] K7 CI green on the branch: `docs/status/write-buffer.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- 2026-10-26 lead: adopt revision 2, including WB-16 to WB-19 and D9's accepted structure.
- CR5 ruling: foreground merges apply pending deletes before scheduling; background merges carry them.
- CR6: choose a 1 KiB first lane chunk.
- CR9: replace `full()` with capacity-aware `room_for`; freeze before a transaction cannot fit.
- CR8: byte equivalence applies only when a flush covers the whole buffer.
- Risk high: Changes concurrent write ownership, buffer visibility, and deletion handling (the lead's mapping).

## May land early

None.
