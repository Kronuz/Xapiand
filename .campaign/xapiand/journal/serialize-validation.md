# serialize-validation: journal

State: waiting on jobs 76dfe9 159564 7f59ed b9fd21 222d40 039ba1 50ab8b
Quest: .campaign/xapiand/quests/serialize-validation.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Branch rewritten 11-18 23:25 (the test commit had a raw-string typo that stopped the suite build, job 636a02, and
the type-name test's random words now exclude names before counting 10,000): tests Xapiand:cdcabfbae; fixes
A1 Xapiand:e50891de3, A2 Xapiand:b5aba4703, A4 Xapiand:ef908749b, A3 Xapiand:8f68bbd53.
A2 mutant (head with A2 reverted): Xapiand:21623ff47 at refs/mutants/serialize-validation/a2-type-names.
Local build in the worktree's `.scratch/serialize-validation/build`: negative 34, time casts 61, geo 4469 checks pass
at head; type names rebuilt after the word-count fix. Then jobs from .scratch/xapiand/serialize-validation/submit.sh
(home) and `campaign verify`.
Jobs: fails-without 76dfe9 (A1 at e872b2f03), 159564 (A2 mutant 21623ff47), 7f59ed (A4 boolean at b5aba4703),
b9fd21 (A4 time casts at b5aba4703), 222d40 (A3 at ef908749b); head pass 039ba1 (A1-A4); suite 50ab8b (A6).

## Next

1. Read the jobs (all compiled and passed locally at head: negative 34, type names 149, time casts 61, geo 4469 checks; test_serialize, test_range_terms, test_geospatial pass); fix what fails.
2. A3 note in docs once the file set has a docs path (Blocked on 2).
3. After coverage-areas lands and is merged here: flip its pins in tests/test_coverage_serialize.cc (lead merges; the file then needs to be in my set or the lead flips it).
4. Correctness review (high risk).

## Blocked on

1. **A5, owner's decision** (`_date`, `_multiconvex`, `_lua` casts). Findings:
   - `_date` is already an alias: `Cast::cast` serves it as datetime (src/cast.cc:61), `FieldType::date` exists and serializes exactly as datetime (`Serialize::serialize`, date and datetime share a case), docs show it as a cast (reference-guide/search/query-dsl/leaf-queries/casting-values.md:119). Only `Serialize::object`, `guess_type`, `guess_serialize` (mine) and `Cast::get_field_type` (src/cast.cc:417, not in my set; the schema's index path at schema.cc:5235 calls it) refuse it. Recommend: serve, `_date` -> FieldType::date with datetime bytes; needs src/cast.cc in the file set.
   - `_multiconvex` is served everywhere but the map form: MultiConvex geometry, EWKT `MULTICONVEX(...)` (src/geospatial/ewkt.cc:194), collection and intersection members, docs (geospatial-type.md:199, casting-values.md:108). `GeoSpatial(const MsgPack&)` (src/geospatial/geospatial.cc:117-160, geo-correctness's files) has no `_multiconvex` case, and none for `_multichull` either, though the serializer accepts `_multichull` and then fails in GeoSpatial. Recommend: serve both map forms (a `make_multiconvex` beside `make_multicircle`) in geo-correctness, not remove: removal would strand the EWKT form and the docs.
   - `_lua` is a script, not a value: `Cast::get_field_type` maps it to FieldType::script and the scripting docs use it (reference-guide/documents/scripting.md:57). A script is never a stored value, so the serializer refusing it is right. Recommend: keep the refusal with a message saying so; nothing to remove.
   Lead's default (serve `_date` if an alias, remove the others if nothing serves them) gives: serve `_date`; `_multiconvex` and `_lua` are both served elsewhere, so neither is removed. Not implementing until a Decisions row exists.
2. **A3's docs note needs a path in my file set.** The set has no docs path (docs/design/geo* is geo-correctness's). Ask: add `docs/mechanisms/geo-exact-term.md`. Draft under Result.
3. **The docs E2E pins A3's bug.** reference-guide/search/query-dsl/compound-queries/geospatial-searches.md:18-70 queries the exact point (41.50343, -74.01042) and expects count 10 with `_id` 1..10: every point matched every point. In docs/public/assets/accounts.json exactly three accounts sit there, `_id` 156, 768, 891, so after the fix the example answers count 3 with those ids. docs/src is not in my set: the lead adds it (I change the expectations) or flips it.
4. **Out of my file set, for the lead** (same bug class as A2: a `case _.fhh(k)` whose key the phf table lacks is npos and catches every miss):
   - src/aggregations/aggregations.cc:217 `case _.fhh(RESERVED_AGGS_AGGS)` is not in the table (line 64), so every unknown aggregation type is taken as `_aggs` instead of "Aggregation type ... is not valid".
   - The cause in one place: constexpr-phf's `find` could fail compilation when a constant-evaluated lookup misses (`if (std::is_constant_evaluated() && miss) throw`), so such a case label cannot compile. Audit script: `.scratch/xapiand/serialize-validation/phf_audit.py` (home) found exactly these three in src/.
   - src/cast.cc:138 (`Cast::cast(FieldType::empty, text)`) has the strtoull wrap I fixed in serialize.cc: "-18446744073709551615" casts to 1.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | pending |
| A2 | pending |
| A3 | pending |
| A4 | pending |
| A5 | Blocked on 1 |
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
