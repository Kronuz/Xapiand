# strata-engine-callbacks (S3, phase 7a)

> Status: CURRENT (2026-10-27) workstream `strata-engine-callbacks`; branch `phase7a/strata-engine-callbacks` from `phase7a/strata-engine-search` (Xapiand:90cfd9775). Local only, not pushed, no CI run.

Design: `strata/docs/design/xapiand-on-strata.md` (strata:8972cf8), row S3, D4, G4, G5, G6, G8.

## State

In progress: first build pending.

## Done criteria (design row S3)

The strata engine runs the query operators P3 provides, expansions over the dictionary, host
sources, key makers and spies; term lists back `_terms` and the info endpoints.

1. SYNONYM, MAX, XOR, ELITE_SET (and the existing SCALE_WEIGHT) on the strata engine, with Xapian's
   scoring; a nested XOR and an elite set chosen by the whole index's frequencies.
2. WILDCARD (prefix, `?`, `*`) and EDIT_DISTANCE nodes expand over the segments' dictionaries once per
   search; `max_expansion` and the three limit modes as Xapian's; the combiner SYNONYM, OR or MAX.
3. Posting sources (`MultipleValueRange`, `MultipleValueGE`, `GeoSpatialRange`) run as a column scan of the
   values plugin; an undecodable value drops its document.
4. A key maker orders hits by key, then relevance, then docid in the requested order.
5. A spy sees every matching document once, with its weight and a lazy `DocView`, whatever `check_at_least`
   says; the estimate stays the exact match count.
6. `DocView` values read from the values plugin (S1's gap closed); term lists back `terms`, `term_details`
   and `serialise` (P2).
7. Tests shared with the Xapian engine: same documents, order, weights (within the stated tolerance) and
   aggregation values on both engines, one and five shards; a mutant per rule; ASan.
8. `harness/numeric_range_check.py` and the aggregation checks on a strata-backed index.
9. `docs/mechanisms/strata-callbacks.md`.

## Done

(nothing committed yet)

## Next

Wire the values plugin, the document view, sorter and spy collection, sources, operators, expansions; tests.

## Blocked on

Nothing. Inputs coded against: prospect P3 (`phase4/xapian-ops`), P1 values (`phase4/values-bytes`) through a
scratch merge of both (`.scratch/prospect-s3`, strata at `phase4/values-rank`), P2 term lists
(`phase4/term-lists`, nothing committed yet), S2 (rebased on at 90cfd9775).

## Log

- 2026-10-27: branch created from S2's head. Scratch build dir `.scratch/build` configured against the merged
  prospect (xapian-ops + values-bytes) and strata `phase4/values-rank`.
