# phase3-lifecycle: Phase 3's lifecycle rows and its landed Track P items

Risk: high. History: this landed before the campaign, under the earlier campaign skill. The lifecycle rows passed at strata:2a111373 (2026-10-14, the architect's fourth re-check) and the last Track P item committed to Phase 3 that landed, P1's crash points, at strata:950d24d (2026-10-17), the commit recorded here. No ledger existed then: strata's `docs/PLAN.md` "Phase 3", `docs/gates/phase-3-lifecycle.md`, `docs/reviews/phase-3-lifecycle.md`, prospect's `docs/gates/phase-3-ratchets.md` and the `docs/perf/` entries are the record. The ratchet rows are `phase3-ratchets`. Its machine state is in `state.json`: `campaign status phase3-lifecycle`.

## Outcome

strata's index has a lifecycle: many segments, snapshots, refresh, commit manifests, tiered merges in the background, primary keys, docids, versions, deletes, metadata, atomic batches, a node-wide memory budget, deterministic apply, checkpoint and install with a logical-content digest, all in format 12. It survives power loss at every barrier, fails closed when the disk fills, and replays to the same digest. Four Track P items committed to Phase 3 landed with it: reordering at merge (P1), the multi-segment query path (P6), the term dictionary (P12) and the verified-region size (P15).

## Acceptance criteria

Carried from strata `docs/PLAN.md`, "Phase 3: index lifecycle": its items, the lifecycle rows of its gate, the "Every gate" rows for the lifecycle code, and the Track P items committed to Phase 3 that landed, each shown by the gate report or its `docs/perf/` entry as an artifact.

- A1 [artifact] Multiple segments, snapshots, refresh, commit manifests, tiered merges: `docs/gates/phase-3-lifecycle.md`
- A2 [artifact] Primary keys, docids, updates, deletes, versions, the live version map, metadata, atomic batches with cancel, the node-wide memory budget: `docs/gates/phase-3-lifecycle.md`
- A3 [artifact] Deterministic `apply`, `checkpoint` and `install`; a logical-content digest: `docs/gates/phase-3-lifecycle.md`
- A4 [artifact] `faultfs`: `docs/gates/phase-3-lifecycle.md`
- A5 [artifact] Format 12, one version for Phase 3's format work, frozen before merge code, with its fixture and a test that format 11 is rejected: `docs/gates/phase-3-lifecycle.md`
- A6 [artifact] Gate: power-loss injection at every barrier of a randomized workload, across at least 10,000 crash points, with zero acknowledged-write loss and zero inconsistent opens: `docs/gates/phase-3-lifecycle.md`
- A7 [artifact] Gate: the digest is equal across two replays: `docs/gates/phase-3-lifecycle.md`
- A8 [artifact] Gate: disk full: fail-closed writes, no corruption, recovery once space returns: `docs/gates/phase-3-lifecycle.md`
- A9 [artifact] Every gate, for the lifecycle code: tests under ASan, UBSan and TSan; one CPU-hour of fuzzing per new or changed decoder, zero findings; a fixture per format change; line and branch coverage at TESTING.md's targets: `docs/gates/phase-3-lifecycle.md`
- A10 [artifact] Every gate: every corruption check of the new decoders reached by a resealed unit test or the replayed corpus (Phase 2 condition C13): `docs/gates/phase-3-lifecycle.md`
- A11 [artifact] Track P1: document reordering by recursive graph bisection at merge time, measured against Lucene's own `-bp`: `docs/design/reordering.md`
- A12 [artifact] Track P6: the per-query fixed cost over many segments measured and cut: `docs/perf/p6-multisegment.md`
- A13 [artifact] Track P12: the term dictionary measured against `.tim` plus `.tip`: `docs/perf/p12-term-dictionary.md`
- A14 [artifact] Track P15: the verified-region size, recorded in the table of contents: `docs/perf/p15-region-size.md`

## Contracts

None recorded: landed work; format 12 and the lifecycle API are in the code and in `docs/design/phase-3.md`.

## Decisions

- Lifecycle review (strata `docs/reviews/phase-3-lifecycle.md`, 2026-10-14): REJECT four times (L1 to L8), then the fourth re-check: "APPROVE WITH CONDITIONS: the lifecycle rows pass. Phase 3R may start once Track C's C1 to C4 pass"; condition 7 (power loss across background merges) closed by the 10,000-point run at strata:e36a793c (2026-10-15).
- The Phase 3 gate is split: the lifecycle rows here; the ratchet rows, carried by the owner (2026-10-15 and 2026-10-16), are `phase3-ratchets`.
- Risk high: durability and an on-disk format (this audit's mapping).

## May land early

None.
