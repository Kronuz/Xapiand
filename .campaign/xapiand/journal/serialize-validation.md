# serialize-validation: journal

State: working
Quest: .campaign/xapiand/quests/serialize-validation.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Fixes committed: A1 Xapiand:77324bbed, A2 Xapiand:e121732c6, A4 Xapiand:f06c5fb55, A3 Xapiand:5e3078b65 (tests at
Xapiand:f12634566). A2 mutant (head with A2 reverted): Xapiand:3a04b5dad, kept at refs/mutants/serialize-validation/a2-type-names.
Jobs queued: suite 636a02 (A6); fails-without 2af582 (A1 at c87aaf555), e5d3b2 (A2 mutant), e023eb (A4 boolean at
e121732c6), c474be (A4 time casts at e121732c6), e13d5d (A3 at f06c5fb55); head pass cb35db (A1-A4).
Local build in the worktree's `.scratch/serialize-validation/build` (iteration only).

## Next

1. Read the jobs; fix what fails (compile not yet checked locally when queued).
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
3. **Out of my file set, for the lead** (same bug class as A2: a `case _.fhh(k)` whose key the phf table lacks is npos and catches every miss):
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

## Rulings I made

- "-0" for a positive is refused with the other negatives: a positive's text has no '-', the rule `Cast::positive` already applies (src/cast.cc:223).
- Type names stay case-insensitive (the tables hash with `hhl`): "Text" is the type text, not an unknown word, though coverage-areas listed it as one.
