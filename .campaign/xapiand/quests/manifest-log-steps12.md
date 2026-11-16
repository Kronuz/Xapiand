# manifest-log-steps12: Manifest framing and codec

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at strata:591658a (2026-11-04); its ledger `docs/status/manifest-log-steps12.md` is the record. Its machine state is in `state.json`: `campaign status manifest-log-steps12`.

## Outcome

Journal framing reads and emits manifest-log records, and strata encodes, decodes and replays snapshots and edits. Replay validation rejects invalid segment identities, lowered next-segment values and malformed canonical encodings before state changes.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] Unit tests with fixtures for framing and codec: `docs/status/manifest-log-steps12.md`
- A2 [artifact] tail_model single-bit-flip property: 219,672 flips, 0 silent, 584 in padding: `docs/status/manifest-log-steps12.md`
- A3 [artifact] Every honest crash image: 1,020 images, 672 torn, 0 misread: `docs/status/manifest-log-steps12.md`
- A4 [artifact] LibFuzzer decoder target runs 5 minutes under ASan+UBSan: `docs/status/manifest-log-steps12.md`
- A5 [artifact] Step 1 revert checks and mutants: `docs/status/manifest-log-steps12.md`
- A6 [artifact] Linux VM tests for journal and strata: `docs/status/manifest-log-steps12.md`
- A7 [artifact] How-it-works notes in `docs/mechanisms/`: `docs/status/manifest-log-steps12.md`
- A8 [artifact] TSan is unchecked because framing and codec are pure functions: `docs/status/manifest-log-steps12.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Segment ids start at 1; replay refuses id 0 and lowering `next_segment`.
- Records use canonical shortest-form integers and canonical deleted-set encodings.
- The deleted set is represented as ascending ordinals, not 64-bit words.
- Risk high: Introduces persistent manifest-log framing and decoding (the lead's mapping).

## May land early

None.
