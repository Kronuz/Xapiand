# serialize-validation: journal

State: working
Quest: .campaign/xapiand/quests/serialize-validation.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Session 3 (11-18 23:40): D130 and A7. Tests Xapiand:bb38411df (cast_negative, aggregation_types, date_lua), docs
Xapiand:c95f679d0 (geo-exact-term.md, the docs example's 3 hits). Fixes next: D130 (cast.cc, serialize.cc), A7 cast
(cast.cc:138), A7 aggregations (aggregations.cc:217); local build in `.scratch/serialize-validation/build`.
Earlier: tests Xapiand:cdcabfbae; fixes A1 Xapiand:e50891de3, A2 Xapiand:b5aba4703, A4 Xapiand:ef908749b, A3
Xapiand:8f68bbd53; A2 mutant Xapiand:21623ff47 (refs/mutants/serialize-validation/a2-type-names).
Jobs: fails-without 76dfe9 (A1), 159564 (A2 mutant), 7f59ed (A4 boolean), b9fd21 (A4 time casts), 222d40 (A3); head
pass 039ba1 (A1-A4); suite 50ab8b (A6, at bfd71aa84: superseded by a suite at the new head).

## Next

1. Commit the D130 and A7 fixes, build and run the three new tests, then submit their fails-without jobs and a suite at head.
2. Read the jobs; fill Evidence; push for CI.
3. After coverage-areas lands and is merged here: flip its pins (lead): tests/test_coverage_serialize.cc lines 558-562,
   642-643, 704-705 expect `_date` and `_multiconvex` refused; `_date` is now served, `_multiconvex` serializes once
   geo-correctness serves its map form, `_lua` stays refused.
4. Correctness review (high risk).

## Blocked on

Nothing. (A5: D130 ruled; docs path and docs page: added to the file set.)

For the lead, out of my file set (same strtoull wrap as A1, not Cast): src/database/handler.cc:107 reads a docid
"&-1" through strict_stoull (wraps); src/query_dsl.cc:579,595 and src/database/schema.cc:1602,1611 read accuracy
suffixes (`_geo-1`, `_-1`) the same way; src/server/search_views.cc:3156,3212 read Content-Length with std::strtoull
("-1" wraps to 2^64-1).

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | pending |
| A2 | pending |
| A3 | pending |
| A4 | pending |
| A5 | D130 (PLAN.md Decisions): `_date` served through Cast, `_multiconvex` served (map form in geo-correctness), `_lua` refused as a script |
| A6 | pending |

## Result

Draft of the A3 note (docs/mechanisms/geo-exact-term.md once allowed):

A geo field's whole geometry is one exact-match term, `Serialize::ranges_hash` of its trixel ranges: the first 16
bytes of SHA-256 over the ranges' fixed 14-byte encodings. Users of those bytes: the indexer (Schema::index_item,
FieldType::geo, the field and global terms, ctype 'G', with the field's partials and error) and the exact-match
query (`get_regular_query` -> `Serialize::MsgPack` -> `object`/`string` -> `geospatial`, and `get_global_query`
through `guess_serialize`). Not users: sorting (centroids in the value slot, keymaker), aggregations (the value
slot's ranges), geo range and within queries (accuracy trixel terms and the value slot), all written by
`ranges_centroids` and `GenerateTerms::geo`, which this does not change.

Cost of the new term (.scratch/xapiand/serialize-validation/hash_bench.cc, home; quiet strict beside 2 light holds,
i9-9980HK, portable SHA-256 kernel: this CPU has no SHA-NI): 437 ns per one-range term (a point), 968 ns for 8
ranges, 7.2 us for 64, 55 us for 512, about 107 ns per range once past the first block; the xor it replaces was
about 1 ns per range. Not yet set against the rest of a geo document's indexing (getRanges, accuracy terms).

## Rulings I made

- "-0" for a positive is refused with the other negatives: a positive's text has no '-', the rule `Cast::positive` already applies (src/cast.cc:223).
- SHA-256 for the geo term: the only hash in the tree with a documented bound (birthday bound on a 128-bit truncation) that holds for any input; a 64-bit hash would collide with probability about 2.7% at 10^9 distinct geometries.
- The type-name tests count 10,000 random words that are no name, from a fixed seed.
- guess_serialize's map branch now goes through `Serialize::object` (one cast switch instead of two copies), which is what fixed `_time`/`_timedelta`; a number out of time's range is now refused there as the field refuses it.
- Type names stay case-insensitive (the tables hash with `hhl`): "Text" is the type text, not an unknown word, though coverage-areas listed it as one.
