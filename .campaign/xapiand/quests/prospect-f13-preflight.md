# prospect-f13-preflight: Format 13 seeds cost and loss attribution

Risk: normal. History: this landed before the campaign, under the earlier campaign skill, at prospect:b60af99 (2026-10-20); its ledger `docs/status/f13-preflight.md` in Kronuz/prospect is the record. The quest's repository is `../prospect`. Its machine state is in `state.json`: `campaign status prospect-f13-preflight`.

## Outcome

M20's `seeds_of` writes byte-identical seed pairs at format 12's cost, the pairs are reproducible across platforms by exact integer comparisons, and every 5M loss class against Tantivy, PISA and Lucene is attributed, classified and ranked by expected gain.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] K1 segment bytes identical to main's at 100k and 1M (flushed, docid merge, reordering merge); suite passes: `docs/status/f13-preflight.md`
- A2 [artifact] K2 builds and docid merges within 1.03 of 026ee10 at 1M (1.002, 1.005), then 5M pairs on the gate VM (mean 1.012, noise about the effect): `docs/status/f13-preflight.md`
- A3 [artifact] K3 every loss class attributed with counters and a profile, in `docs/perf/format13-loss-attribution.md`: `docs/status/f13-preflight.md`
- A4 [artifact] K4 FQ3's phrase cost with a level-1 positional offset measured (0.988 COUNT, 0.983 TOP_1000, +253,760 bytes at 1M): `docs/status/f13-preflight.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Reproducible seeds (lead, 2026-10-20): exact 128-bit integer comparisons, `pre_freeze` 0x80000008.
- Owner: no freeze until the first release; the attribution ranked by expected gain (7b3bcca).
- For the lead: whether the 5M build pairs need a quieter host to read below the +-8% noise (open).
- Risk normal: writer-side change that must keep segment bytes (the auditor's mapping).

## May land early

None.
