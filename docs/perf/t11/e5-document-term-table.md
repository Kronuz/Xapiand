# T11 E5: one term table per document

Target: strata engine row of gate 7a T11 (see e1-flat-document.md).

## Footprint

After E4, a 5-round interleaved run puts the strata engine row at 0.502x Xapian's CPU (0.189 against 0.377 ms per
document; pairs 0.546 0.479 0.493 0.509 0.484), just over the 0.5 target. A document is 171 terms in 47 fields, and each
field owns a terms vector (and, past 8 terms, a table): `~StrataDocument` is 216 samples and `StrataDocument::intern`
418 of a row of about 4200.

## Hypothesis

One terms array and one open-addressing table per document, keyed by (field, term bytes), replace the 47 per-field
vectors with two blocks. `intern` then touches contiguous memory and the destructor frees a handful of blocks; `fill`
and `serialise` group the terms by field with a counting pass (the document keeps arrival order). Expected: 4 to 6% of
the row.

## Procedure

1. Unit tests engine_fields, engine_strata, engine_document, engine_shard, engine_replay.
2. Docs E2E, callbacks check, write check.
3. T11: E4 against E5, 5 interleaved pairs with an A/A, then the final Xapian, strata master, strata new interleave.

Keep if the row falls beyond the A/A spread; remove if not.
