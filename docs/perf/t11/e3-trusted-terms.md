# T11 E3: skip the index's term re-check for terms the host builds from a table

Target: strata engine row of gate 7a T11 (see e1-flat-document.md).

## Footprint

After E1 and E2, `prospect::inverted_plugin::valid_field` is 277 samples of a strata row of about 5100, all on the
writer threads (DI, under the shard lock): per field of every document it copies the terms' views, sorts them to find a
repeated term and checks positions are increasing.

## Hypothesis

The strata document finds a term through a hash table, so a term is unique by construction, and `normalise()` makes
positions strictly increasing before `fill`. Only the term size limit is left to check, which `fill` does in the loop it
already runs. An additive `field_part::checked` (prospect) lets stage 1 skip the sort for such a part; a part with
tokens or split text is still checked in full. Expected: about 5% of the row.

## Procedure

1. Unit tests engine_strata, engine_shard; the terms cap harness (`harness/terms_cap_check.py`) still refuses an
   oversized term.
2. Docs E2E and callbacks check against the E2 binary.
3. T11 profile: E2 against E3, 4 interleaved pairs with an A/A, `quiet exclusive`.

Keep if the row falls beyond the A/A spread; it removes a safety check the index would otherwise make, so the change
is kept only if it is worth more than the noise.
