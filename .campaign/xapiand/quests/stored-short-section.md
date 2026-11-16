# stored-short-section: Test rejection of short stored sections

Risk: low. History: this landed before the campaign, under the earlier campaign skill, at strata:75716f0 (2026-10-26); its ledger `docs/status/stored-short-section.md` is the record. Its machine state is in `state.json`: `campaign status stored-short-section`.

## Outcome

Format tests reject truncated stored sections, while fuzzing reaches the short-section case and traps if a document-bearing segment opens. The reader behavior itself remains unchanged.

## Acceptance criteria

The ledger lists no explicit done criteria; these are its Done items.

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] Reject stored sections of 0, 1, 2, 3, 4, 5, and 20 bytes and the 4-byte tail; allow a segment without documents to open: `docs/status/stored-short-section.md`
- A2 [artifact] (no explicit criteria; from the Done list) Add fuzz short-section selection, an oracle for sections under 21 bytes, and 25 corpus seeds: `docs/status/stored-short-section.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- None recorded in the ledger.
- Risk low: Adds test and fuzz coverage without changing reader behavior (the lead's mapping).

## May land early

None.
