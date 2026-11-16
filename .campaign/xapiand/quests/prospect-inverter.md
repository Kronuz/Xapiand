# prospect-inverter: Tuple-stream inverter on layers

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at prospect:e14508c (2026-10-18); its ledger `docs/status/inverter.md` in Kronuz/prospect is the record. The quest's repository is `../prospect`. Its machine state is in `state.json`: `campaign status prospect-inverter`.

## Outcome

The engine's indexing front end is the research note's inverter C as a tuple buffer fed with token streams, rebased onto layers' plugins (R23, R24), measured experiment by experiment on T4's one-thread row against Tantivy and Lucene, with each loser removed.

## Acceptance criteria

No explicit criteria ("none set yet" in the ledger); from its Done list and verification, each shown by the ledger as an artifact.

- A1 [artifact] Rebase onto layers: public additions limited to `document::text`, `tokens`, `split_text` and `text_token`; R23 and R24 applied: `docs/status/inverter.md`
- A2 [artifact] Byte-identical segments, manifests and meta to `ref-base-20k` and `ref-base-100k`; ctest 35 of 35 at 888d03c: `docs/status/inverter.md`
- A3 [artifact] Review findings folded in: CR1, CR2, CR9, CR10, CR11 (each with its test): `docs/status/inverter.md`
- A4 [artifact] Experiments E0 to E37 recorded in `docs/perf/inverter.md`, each kept or removed by BENCHMARKS rule 3: `docs/status/inverter.md`
- A5 [artifact] Gate VM: 1M build 3.494x Tantivy (3.207-3.498) at c6bf4d9; 5M round 192.2 s: `docs/status/inverter.md`
- A6 [artifact] CI run 39189683111 green on the jobs recorded: `docs/status/inverter.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Owner direction (2026-10-17): for every cost try at least one mechanism neither Lucene nor Tantivy uses.
- Main's 2ec8c36 and 1e2d89b (strata numbering) do not build; a bisect skips them (R26, CR19).
- Risk high: the segment writer every index uses; bytes must stay identical (the auditor's mapping).

## May land early

None.
