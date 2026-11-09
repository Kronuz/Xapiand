# phase2: Phase 2, single-segment engine

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at strata:6932b812 (2026-10-13), the commit that records the architect's approval of the Phase 2 gate. No ledger existed then: strata's `docs/PLAN.md` "Phase 2", `docs/gates/phase-2.md` and `docs/reviews/phase-2-gate.md` (prospect's since the repository split of 2026-10-20) are the record. Its machine state is in `state.json`: `campaign status phase2`.

## Outcome

A single-segment engine writes and reads segments with term blocks, postings, positions, lengths and stored records, and answers term, boolean, phrase and near queries with Xapian's BM25 and a Lucene-compatible BM25, pruned by block-max MaxScore and WAND, with top-k and exact counts. It beats Xapian on every class and on single-thread indexing, stays within 1.5x of the faster of Lucene and Tantivy on every class, and matches the oracle's match sets and top-10 scores.

## Acceptance criteria

Carried from strata `docs/PLAN.md`, "Phase 2: single-segment engine", its items and gate rows, and the Phase 2 rows of the Ratchets and "Every gate", each shown by the gate report as an artifact.

- A1 [artifact] In-memory segment writer and flush; the segment file with term blocks, postings, positions and lengths: `docs/gates/phase-2.md`
- A2 [artifact] Segment reader on the residency interface; term, boolean, phrase and near queries: `docs/gates/phase-2.md`
- A3 [artifact] BM25 with Xapian's formula and a Lucene-compatible BM25; block-max MaxScore and WAND; top-k collection; exact counting: `docs/gates/phase-2.md`
- A4 [artifact] `termdict`: the block-tree term dictionary over `fst`: `docs/gates/phase-2.md`
- A5 [artifact] The Xapian-compatible analyzer for text fields, checked against the oracle vectors: `docs/gates/phase-2.md`
- A6 [artifact] Scorer contract additions: the minimum competitive score passed into nested scorers; 64 KiB region checksums reaching `residency`: `docs/gates/phase-2.md`
- A7 [artifact] Gate: Xapian is a floor: strata wins every class and single-thread indexing: `docs/gates/phase-2.md`
- A8 [artifact] Gate: `TOP_10`, `TOP_100`, `TOP_1000` (Lucene-compatible BM25) within 1.5x of the faster of Lucene and Tantivy per class; a class beyond 2x blocks: `docs/gates/phase-2.md`
- A9 [artifact] Gate: `COUNT` and `TOP_100_COUNT` within 1.5x of the reference per class: `docs/gates/phase-2.md`
- A10 [artifact] Gate: the term class gains 200 single-term queries stratified by document frequency: `docs/gates/phase-2.md`
- A11 [artifact] Gate: single-thread indexing at least 1.0x Lucene and 0.8x Tantivy with equal settings: `docs/gates/phase-2.md`
- A12 [artifact] Gate: the whole postings section at most 1.02x Lucene's `.doc` on Wikipedia: `docs/gates/phase-2.md`
- A13 [artifact] Gate: `termdict` exact lookup (present and absent keys) at most 1.0x Lucene's `seekExact`: `docs/gates/phase-2.md`
- A14 [artifact] Gate: relevance parity: identical boolean match sets against the oracle; identical BM25 top-10 without deletes, relative score error at most 1e-9: `docs/gates/phase-2.md`
- A15 [artifact] Every gate: tests under ASan and UBSan, and TSan where threads are involved; one CPU-hour of fuzzing per decoder, zero findings; readers reject unknown versions; a fixture per format change: `docs/gates/phase-2.md`
- A16 [artifact] Ratchets, Memory, Phase 2: indexing peak RSS, serving RSS with the index open, and heap bytes and allocations per query by class recorded: `docs/gates/phase-2.md`
- A17 [artifact] Ratchets, Disk, Phase 2: the whole index recorded: `docs/gates/phase-2.md`

## Contracts

None recorded: landed work; the segment format (format 11 at the gate) and the scorer contract are in the code and in `docs/design/phase-2.md`.

## Decisions

- Gate review (2026-10-12 to 13): REJECT on G1 to G5, two re-checks, then the third re-check's ruling: "APPROVED WITH CONDITIONS. The Phase 2 gate passes."
- Conditions C1 to C3, C5, C7, C9 and C13 to C16 carry into Phase 3 by their stated points (quest `phase3-ratchets` A10); C4, C6, C8, C10 and C12 closed; C11 superseded by the Phase 3 disk ratchet.
- The PLAN amendments (A1 to A31, strata:a19a3373) landed after this gate's evidence: their "Every gate" items apply from the Phase 3 gate (C16).
- Risk high: an on-disk format and decoders of untrusted bytes (this audit's mapping).

## May land early

None.
