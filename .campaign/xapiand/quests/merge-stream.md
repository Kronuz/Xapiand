# merge-stream: Stream stored sections during merges

Risk: normal. History: this landed before the campaign, under the earlier campaign skill, at strata:6589f32 (2026-10-21); its ledger `docs/status/merge-stream.md` is the record. Its machine state is in `state.json`: `campaign status merge-stream`.

## Outcome

Merge working memory is bounded by a few blocks and key-builder state rather than merged segment size. Segment bytes remain identical to main's.

## Acceptance criteria

The ledger lists no explicit done criteria; these are its Done items.

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] Stream stored-section frames through section output and reuse the frame writer for flushes: `docs/status/merge-stream.md`
- A2 [artifact] (no explicit criteria; from the Done list) Build key-ordered merge plan without docid-sorted or refs copies; return plan charge and size builders up front: `docs/status/merge-stream.md`
- A3 [artifact] (no explicit criteria; from the Done list) Pin merged-file digests for five shapes and bound merge budget peak: `docs/status/merge-stream.md`
- A4 [artifact] (no explicit criteria; from the Done list) Run Release, ASan+UBSan, and TSan ctest: `docs/status/merge-stream.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- None recorded in the ledger.
- Risk normal: Changes merge memory use without changing segment format (the lead's mapping).

## May land early

None.
