# serialize-validation: journal

State: waiting on jobs 7f59ed b9fd21 222d40 e9f90f 367e06 859ec0 f13d5c f41436
Quest: .campaign/xapiand/quests/serialize-validation.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Session 3 (11-18): D130 and A7 done, docs done. Tests Xapiand:bb38411df (+ Xapiand:ba766afee: the nested-type case
dropped, see rulings); docs Xapiand:c95f679d0; fixes D130 Xapiand:df8cf48b7, A7 cast Xapiand:a1e119179, A7
aggregations Xapiand:101c8ef01; A7 aggregation mutant Xapiand:4a13c96fd (refs/mutants/serialize-validation/a7-aggs).
Locally at Xapiand:ba766afee all seven tests pass (negative 34, type names 149, time casts 61, geo 4469, cast_negative
29, aggregation_types 15, date_lua 20 checks); before the fixes cast_negative failed 2, aggregation_types 5, date_lua
aborted on "Unknown cast type '_date'".
Earlier: tests Xapiand:cdcabfbae; fixes A1 Xapiand:e50891de3, A2 Xapiand:b5aba4703, A4 Xapiand:ef908749b, A3
Xapiand:8f68bbd53; A2 mutant Xapiand:21623ff47 (refs/mutants/serialize-validation/a2-type-names).
Jobs (submit scripts: .scratch/xapiand/serialize-validation/submit.sh, submit2.sh in home): fails-without 76dfe9 (A1,
done, failed as it must), 159564 (A2 mutant, done, failed as it must), 7f59ed (A4 boolean), b9fd21 (A4 time casts),
222d40 (A3), e9f90f (A7 cast at Xapiand:df8cf48b7), 367e06 (A7 aggregations mutant), 859ec0 (D130 at
Xapiand:095b65241, filed under A7 since A5 takes no job); head pass f13d5c (A1-A4, A7, at Xapiand:ba766afee); suite
f41436 (A6). Cancelled as stale: 039ba1, 50ab8b (older head).

## Next

1. Read jobs 7f59ed b9fd21 222d40 e9f90f 367e06 859ec0 f13d5c f41436; fill Evidence; fix what fails.
2. The lead pushes the branch for CI (contributors never push).
3. After coverage-areas lands and is merged here: flip its pins (lead): tests/test_coverage_serialize.cc lines 558-562,
   642-643, 704-705 expect `_date` and `_multiconvex` refused; `_date` is now served, `_multiconvex` serializes once
   geo-correctness serves its map form, `_lua` stays refused.
4. Correctness review (high risk).

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 76dfe9 failed without the fix (exit 1, 11 of 30 checks); head f13d5c pending |
| A2 | 159564 failed at the mutant (exit 1, 22 of 149 checks); head f13d5c pending |
| A3 | 222d40 pending; note docs/mechanisms/geo-exact-term.md (Xapiand:c95f679d0) |
| A4 | 7f59ed, b9fd21 pending; head f13d5c pending |
| A5 | D130 (PLAN.md Decisions): `_date` served through Cast, `_multiconvex` served (map form in geo-correctness), `_lua` refused as a script |
| A7 | pending: e9f90f, 367e06, 859ec0 (fails-without), f13d5c (head) |
| A6 | pending: f41436 (suite); CI after the lead pushes |

## Result

- docs/mechanisms/geo-exact-term.md: the geo term, its collision bound, 437 ns per point, who reads it, old indexes.
- docs/src/.../compound-queries/geospatial-searches.md: the exact point example answers 3 (_id 156, 768, 891).
- tests/test_serialize_validation_{cast_negative,aggregation_types,date_lua}.cc beside the four earlier ones.

## Rulings I made

- "-0" for a positive is refused with the other negatives: a positive's text has no '-', the rule `Cast::positive` already applies (src/cast.cc:223).
- SHA-256 for the geo term: the only hash in the tree with a documented bound (birthday bound on a 128-bit truncation) that holds for any input; a 64-bit hash would collide with probability about 2.7% at 10^9 distinct geometries.
- The type-name tests count 10,000 random words that are no name, from a fixed seed.
- guess_serialize's map branch now goes through `Serialize::object` (one cast switch instead of two copies), which is what fixed `_time`/`_timedelta`; a number out of time's range is now refused there as the field refuses it.
- Type names stay case-insensitive (the tables hash with `hhl`): "Text" is the type text, not an unknown word, though coverage-areas listed it as one.
- D130 also adds `_multiconvex` to Cast::cast, Cast::get_field_type, Serialize::object and guess_type (the Cast half of serving it; geo-correctness owns GeoSpatial's map form, so a `_multiconvex` map serializes once that lands).
- `_lua` is refused with "'_lua' is a script, not a value" (CastError/SerializationError, both 400) in Cast::cast, Serialize::object and guess_type; Cast::get_field_type keeps `_lua` -> script for script fields.
- Cast::cast(empty) skips the positive try for any text with a '-' (as Serialize's positive_of): a negative out of int64 is the float it spells.
- Aggregations: `_aggs` and `_aggregations` join the phf and both skip to the type; a comment key (`#...`) is still skipped, as the old catch-all did, so only non-comment unknown words become refusals.
- A nested unknown type is refused when its bucket builds sub-aggregations (per bucket, at match time), not at parse; validating eagerly would construct each subtree once per ancestor in bucket.h, outside my set. The test covers parse-time refusal only.
- Out of my file set, for the lead (same strtoull wrap, not Cast): src/database/handler.cc:107 (docid "&-1"), src/query_dsl.cc:579,595 and src/database/schema.cc:1602,1611 (accuracy suffixes), src/server/search_views.cc:3156,3212 (Content-Length "-1").
