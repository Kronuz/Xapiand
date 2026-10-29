# T11 E4: the strata document's terms and positions in two per-document arenas

Target: strata engine row of gate 7a T11 (see e1-flat-document.md).

## Footprint

A bulk-restore document has about 171 terms in 47 fields and 137 positions (a temporary counter, removed). After E3
the document costs about 1065 samples to build (`DocField::term` 597, `field_of` 209, `add_posting` 208, `add_value`
51 on the DP threads) and about 470 to free (`DocField` destructor 244, `~basic_document` 136, `~StrataDocument` 89 on
the DI threads), of a strata row of about 5100. Each term owns a heap-or-SSO string and a positions vector, so a document
makes and frees a few hundred small blocks.

## Hypothesis

Term bytes in one per-document byte arena (a term is an offset and a size) and positions in one per-document array of
(position, next) nodes chained per term remove the per-term allocations from the build and from the destruction. The
index still takes a `std::vector` per term, so `fill` makes those (one per term with positions) and nothing else.
Positions are ordered and deduplicated when read (`fill`, `serialise`), which replaces `normalise()`. Expected: 5 to
8% of the row.

## Procedure

1. Unit tests engine_fields, engine_strata, engine_document, engine_shard, engine_replay; the strata fuzz decoder's
   fixed point (`serialise` of an unserialised document is a fixed point).
2. Docs E2E, callbacks check, write check.
3. T11 profile: E3 against E4, 4 interleaved pairs with an A/A, `quiet exclusive`.

Keep if the row falls beyond the A/A spread; remove if not.
