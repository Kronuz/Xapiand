# flush-memory: Stream flushes with borrowed records

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at strata:5685428 (2026-10-21); its ledger `docs/status/flush-memory.md` is the record. Its machine state is in `state.json`: `campaign status flush-memory`.

## Outcome

The flush writer reserves capacity, borrows stored records, and streams every flush shape to its segment file. Segment bytes remain unchanged.

## Acceptance criteria

The ledger lists no explicit done criteria; these are its Done items.

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] Reserve and borrow stored records; stream every run count; `flush_memory_test` confirms the writer holds no copy at its first write: `docs/status/flush-memory.md`
- A2 [artifact] (no explicit criteria; from the Done list) Differential fuzz confirms the borrowing writer equals the copying writer: `docs/status/flush-memory.md`
- A3 [artifact] (no explicit criteria; from the Done list) Pin one-run and three-run flush digests from main: `docs/status/flush-memory.md`
- A4 [artifact] (no explicit criteria; from the Done list) Review fixes CR1-CR4 address thread documentation, owner lease, close gate, and exact-size allocation counts: `docs/status/flush-memory.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- CR5 accepted as is: the single-run flush already took the footer CRC from the mapping.
- Risk high: Changes buffer borrowing and writer lifetimes under concurrency (the lead's mapping).

## May land early

None.
