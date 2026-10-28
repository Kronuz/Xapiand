# Engine census (gate 0.5 rows T1 and T5)

What the docs E2E suite asks of the engine seam, measured on an `ENGINE_CENSUS` build of `phase0.5/census`
(Xapiand master `8170a3ef6` plus the counters). One node, `--solo`, default options (so 5 shards per index), a fresh
data directory; `harness/e2e_capture.sh` with newman 6.2.3 (339 requests, all assertions green: `e2e_assert.py` GREEN).
Each request wrote one JSON line (`XAPIAND_ENGINE_CENSUS=file`; how it works: `docs/mechanisms/engine-census.md`). Rerun:

```sh
cmake -S . -B build-census -DCMAKE_BUILD_TYPE=Release -DENGINE_CENSUS=ON && cmake --build build-census --target xapiand
PORT=18953 XAPIAND_ENGINE_CENSUS=$PWD/census.jsonl harness/e2e_capture.sh build-census/bin/xapiand ours.json DATADIR
python3 harness/census_report.py census.jsonl ours.json      # the tables below
```

Method notes. Counts are made at the seam (`prepare`, `search`, the `Shard` operations, the `DocView` methods, the host
callbacks behind their adapters, the document a shard is asked to write). A count belongs to a request when exactly one
is in flight; the suite is sequential, so none of the 346 request lines is tainted. 7 lines are the harness's readiness
probes (not suite requests), and 16 lines hold counts made outside any request (startup bookkeeping and the background
commits). The `engine::Document` builders (`add_term`, `add_value`, ...) are not hooked, to keep `document.h` untouched
for the write path branch; a written document is counted by shape instead (terms, positioned terms, values, data).
Single capture; counts of requests are exact, counts of calls depend on the data.

## Result

| Row | Target | Result |
|---|---|---|
| T1 census | per E2E request, the facilities used | 339 suite requests in 339 lines; the tables below. 94 reach `engine::search`, 142 write documents, 11 touch no engine facility |
| T5 key prefix | at most one term leads with the key prefix and it is the id term; none of the accuracy, partial-path or `exists` terms | **Not met as designed, in two ways.** (1) The key prefix is not `_id.`: no document of 2625 has a term leading with `_id.`; the id term leads with `Q` (`QK` + a keyword id, `QN` + a numeric one). (2) With prefix `Q`, 2623 of 2625 documents hold exactly one `Q` term and it is the key term; 2 documents (both in `RESTORE /twitter/tweets/`, whose ids are the integers `"_id": 1` and `2`) hold 7 `Q` terms each: the id term and six accuracy terms of the numeric id (`Q<186a0>.N...`, `Q<2710>.N...`, `Q<3e8>.N...`, `Q<5f5e100>.N...`). Partial-path and `exists` terms never lead with `Q` in the suite |

### T5 in numbers

| Counter | `Q` | `_id.` |
|---|---|---|
| documents written | 2625 | 2625 |
| no term with the prefix | 0 | 2625 |
| exactly one term, the write named the key and it is that term | 473 | 0 |
| exactly one term, the write named no key (replace by docid) | 2150 | 0 |
| a second term with the prefix | 2 | 0 |
| key does not lead with the prefix | 0 | 473 |

The 2150 writes by docid carry no key, so for them only "one `Q` term" is shown, not "it is the id term". The writes
that name a key (`replace_by_key`, 473) include the schema documents (`QK.xapiand/indexes`, `QKtwitter/user`, ...), which
are keyed like user documents. Conclusion for the design: the keyed index needs a key prefix of `Q` (or the handler must
pass the key and a longer, unambiguous prefix); a numeric id field with accuracy terms breaks "at most one"; D5's
`_id.` expectation (`schema.cc:91`) is wrong because the id term is built from `DOCUMENT_ID_TERM_PREFIX` (`utils.h:61`).

## T5 after amendment A-T5 (2026-10-27, `phase0.5/seam-census`)

The branch merges this census into the seam write path (`engine::Document` is a handle over the engine's native document)
and moves a numeric `_id`'s accuracy terms from the `Q` prefix to `A` (`acc_term_prefix` in `src/database/utils.cc`). The same
suite, one node, `--solo`, an `ENGINE_CENSUS` Release build (`harness/e2e_capture.sh`, newman 6.2.3, 576 of 576 assertions):

| Counter | Before (`8170a3ef6`) | After |
|---|---|---|
| documents written | 2625 | 2625 |
| exactly one `Q` term, the write named the key and it is that term | 473 | 473 |
| exactly one `Q` term, the write named no key | 2150 | 2152 |
| more than one `Q` term | 2 | **0** |

`harness/id_terms_check.py BIN [--census]` repeats the case outside the suite: an index with an integer `_id`, 97 ids from 1
to 3000000000, 20 range queries over `_id` checked against a brute-force answer, and the T5 count (87 documents with more than
one `Q` term before, 0 after). Indexes with a numeric `_id` written before the change keep the old terms and need reindexing
for range queries over `_id`. One id the harness leaves out (`ID_TERMS_EXTRA=268435456` adds it): it is missing from the answer to the range `0..3000000000`
on the seam write binary and on this branch alike (a coarse-range gap in the range-term generator, not looked into here).

## What the suite uses, by facility

Requests are the 339 suite requests (96 are `SEARCH`; 94 of them reach `engine::search`). "Calls" are
counts of the seam call.

### Queries (95 `prepare` calls, one in each of 95 requests; 94 of them then search)

| Facility | Requests | Nodes or calls |
|---|---|---|
| node kind `text` (the engine's query parser) | 39 | 61 |
| node kind `append` | 59 | 184 |
| node kind `combine` | 19 | 68 (29 binary, 39 n-ary) |
| node kind `term` | 17 | 91 (1 request uses a position) |
| node kind `match_all` | 38 | 38 (36 requests are only match-all) |
| node kind `source` | 10 | 12 |
| node kind `wildcard` | 2 | 2 (both with combiner SYNONYM; limits `error` and `most_frequent`) |
| node kind `edit_distance` | 1 | 1 (distance 3, prefix length, combiner SYNONYM) |
| node kind `scale` | 0 | 0 |
| operators AND / OR / FILTER | 59 / 15 / 11 | 153 / 45 / 16 |
| operators AND_MAYBE / NEAR / PHRASE / ELITE_SET | 1 / 1 / 1 / 2 | 4 / 1 / 1 / 32 |
| operators AND_NOT, XOR, SYNONYM (as a combine), MAX, SCALE_WEIGHT | 0 | 0 |
| text flags PHRASE, LOVEHATE, FUZZY, partial_max_expansion | 39 | 61 (set on every text node) |
| text flags WILDCARD / PARTIAL / BOOLEAN | 4 / 3 / 1 | 4 / 3 / 1 |
| text flags WILDCARD_SINGLE, WILDCARD_MULTI, CJK_NGRAM, WORD_BREAKS | 0 | 0 |
| text with stemmer and stop words / without | 9 / 30 | 23 / 38 |
| text default operator AND, ELITE_SET, NEAR, PHRASE | 1 each | 1 each |

### Search requests (94 calls)

| Facility | Requests |
|---|---|
| shards in the search | 5 in all 94 (the default `--shards 5`) |
| spy `AggregationMatchSpy` set | 82 (79 saw a document) |
| `maxitems` 0 (aggregation only) | 29 |
| `check_at_least` set | 82 |
| key maker `Multi_MultiValueKeyMaker` as the sorter | 24 |
| `first` above 0 | 1 |
| collapse, percent threshold, weight threshold, `fuzzy`, `nearest` | 0 |
| docid order other than ascending | 0 |

### Host callbacks

| Callback | Requests | Calls |
|---|---|---|
| `AggregationMatchSpy.see` | 79 | 36808 |
| `Multi_MultiValueKeyMaker.key` | 24 | 6103 |
| source `MultipleValueRange` accept / weight | 6 | 1083 / 522 |
| source `MultipleValueGE` accept / weight | 2 | 18 / 10 |
| source `GeoSpatialRange` accept / weight | 4 | 39 / 19 |
| source `MultipleValueLE`, others | 0 | 0 |

### `DocView` methods, by who calls them

| Method | Calls | By |
|---|---|---|
| `value(slot)` | 34446 | spy 28113, key maker 6103, host reads 230 |
| `terms(prefix)` | 1000 | spy 1000 (one request) |
| `data()` | 1035 | host reads (262 requests) |
| `term_details()`, `values()` | 27, 27 | host (`INFO` of a document) |
| `did()` | 5 | host (`DUMP`) |
| `version()`, `serialise()` | 0 | never called by the suite |

### Shard operations (requests using each)

| Operation | Requests | Calls |
|---|---|---|
| `find_key` | 228 | 16694 |
| `get` | 261 | 1315 |
| `replace` (by docid) | 121 | 2152 |
| `replace_by_key` | 85 | 471 |
| `add` | 1 | 1 |
| `erase` / `erase_by_key` | 2 / 1 | 2 / 1 |
| `commit` | 127 | 146 |
| `set_metadata` / `metadata` | 62 / 62 | 433 / 425 |
| `reopen`, `stats`, `scan` | 13, 3, 2 | 65, 13, 10 |
| `metadata_keys`, spelling, transactions | 0 | 0 |

Written documents: 2625, 75 terms on average (58% with positions), 11 values, a stored body each; the largest request
(a `RESTORE`) wrote 1008.

## Proposed update to the design, section 2

For the lead; `xapiand-on-strata.md` is not edited here. Changes are to the "E2E?" column and the gaps, each backed by a
count above.

| Group | Proposed change | Evidence |
|---|---|---|
| A. Lifecycle | none; add that `reopen` (13 requests) and `stats` (3) are used, transactions and `metadata_keys` are not | shard table |
| B. Documents | `replace_by_key` and `replace` (by docid) are both needed (85 and 121 requests); `add` once; delete by docid or key once or twice. **G2 term lists are needed** (`term_details`, 27 requests, `INFO`); G1 `values()` too. `version()` and `serialise()` of a view are unused by the suite (the WAL reads `Document::serialise`) | shard and DocView tables |
| B/D5. Key prefix | The key prefix is `Q`, not `_id.`. A numeric id field puts its accuracy terms under the key prefix (T5 fails on 2 documents), so strata's keyed index must either accept more than one `Q` term, or the handler must stop giving the id field accuracy terms, or the prefix must be longer (`QK`, `QN` plus a delimiter) **Resolved by A-T5** (see "T5 after amendment A-T5"): the accuracy terms lead with `A`, so one `Q` term per document | T5 table |
| C. Values | `value(slot)` is the hot facility: 34446 calls, 98% from spies and key makers. No typed slots seen | DocView table |
| D. Queries | G9 `queryparser` is certain (39 requests, 61 text nodes; flags PHRASE, LOVEHATE, FUZZY, partial expansion on all; WILDCARD 4, PARTIAL 3, BOOLEAN 1). **G3 shrinks**: ELITE_SET is used (2 requests), XOR, MAX and SCALE_WEIGHT never, SYNONYM only as the combiner of expansions, AND_NOT never as a node (negation lives in the text parser). **G8**: wildcard 2 requests, edit distance 1, partial 3; all three are exercised, `WILDCARD_SINGLE`/`MULTI` and CJK flags are not. NEAR, PHRASE, AND_MAYBE, FILTER once or more as nodes | Queries table |
| E. Enquire | **G7 is needed for the E2E gate: all 94 searches ran over 5 shards** (`opts.cc:43` default `NUM_SHARDS 5`; `opts.h:89`'s `1` is only the struct default). The design's "G7 no (`num_shards = 1`)" does not hold for the default node; either the strata E2E runs with `--shards 1` (a documented difference) or G7 moves up. G5 key-then-relevance: 24 requests. Collapse, thresholds, `fuzzy`, `nearest` (`get_eset`), docid order other than ascending: 0, so they stay in the deferrable list. `check_at_least` is set on 82 searches and `maxitems = 0` on 29: the estimate and aggregation-only searches matter | Search table |
| F. Spies, sources, keys | One spy class (`AggregationMatchSpy`, 79 requests, reads `value` and `terms`), one key maker (`Multi_MultiValueKeyMaker`, reads `value`), three sources (`MultipleValueRange` 6, `GeoSpatialRange` 4, `MultipleValueGE` 2; no `LE`). G4 and G6 are certain | Callbacks table |
| G. Geo | unchanged: through G4 (`GeoSpatialRange`) and G5 | callbacks |
| H. Analysis | stemmer and stop words are used (9 requests); `cjk_words` flags unused | text row |
| I. Remote | n/a (`--solo`), unchanged | |


## Per request class

The tables under this heading and under "Facility totals" are regenerated from an `ENGINE_CENSUS` build of master `7efc3753e` (gate 0.5 run 2: 339 requests, 576 of 576 assertions); the sections above keep their original capture. Call counts moved with the exact aggregations (every match is checked), the exact numeric ranges and the write path that builds documents once.

Class is the method and the path shape (`{index}` the first segment, `{path}` the rest). Facilities are the census keys; `shard:` lists the shard operations. Zero-activity rows touch no engine facility (index state is served from the host).

| Class | Requests | Facilities (requests using each) |
|---|---|---|
| `CLOSE /{index}/` | 1 |  |
| `DELETE /{index}/{path}` | 3 | shard: commit 2, erase 2, erase_by_key 1, find_key 3, replace_by_key 1; `t5[Q].documents` 1, `t5[Q].one_key_term.named` 1, `t5[_id.].documents` 1, `t5[_id.].no_key_term` 1, `t5[_id.].violation.key_without_prefix` 1 |
| `DELETE /{index}/{path}/` | 2 |  |
| `DUMP /{index}/{path}/` | 2 | shard: scan 2; `docview.data` 2, `docview.data@host` 2, `docview.did` 2, `docview.did@host` 2 |
| `GET /` | 1 | shard: find_key 1, get 1; `docview.data` 1, `docview.data@host` 1 |
| `GET /:metrics` | 1 | shard: commit 1 |
| `GET /{index}` | 1 |  |
| `GET /{index}/` | 1 | shard: find_key 1, get 1; `docview.data` 1, `docview.data@host` 1 |
| `GET /{index}/:info` | 1 | shard: stats 1 |
| `GET /{index}/{path}` | 42 | shard: find_key 41, get 41; `docview.data` 41, `docview.data@host` 41 |
| `GET /{index}/{path}/` | 11 | shard: find_key 11, get 11; `docview.data` 11, `docview.data@host` 11 |
| `HEAD /{index}/{path}` | 1 | shard: find_key 1, get 1 |
| `HEAD /{index}/{path}/` | 1 |  |
| `INFO /{index}/` | 1 | shard: stats 1 |
| `INFO /{index}/{path}` | 28 | shard: find_key 28, get 27; `docview.data` 27, `docview.data@host` 27, `docview.term_details` 27, `docview.term_details@host` 27, `docview.value` 27, `docview.value@host` 27, `docview.values` 27, `docview.values@host` 27 |
| `INFO /{index}/{path}/` | 1 | shard: stats 1 |
| `OPEN /{index}/` | 1 |  |
| `POST /{index}/{path}` | 1 |  |
| `POST /{index}/{path}/` | 3 | shard: add 1, commit 3, find_key 3, get 1, metadata 1, replace 1, replace_by_key 2, set_metadata 1; `docview.data` 1, `docview.data@host` 1, `docview.value` 1, `docview.value@host` 1, `t5[Q].documents` 3, `t5[Q].one_key_term.named` 3, `t5[Q].one_key_term.unnamed` 1, `t5[_id.].documents` 3, `t5[_id.].no_key_term` 3, `t5[_id.].violation.key_without_prefix` 3 |
| `PUT /{index}/` | 10 | shard: find_key 10, get 10, metadata 5, reopen 9, replace 7, replace_by_key 7, set_metadata 6; `docview.data` 10, `docview.data@host` 10, `docview.value` 7, `docview.value@host` 7, `t5[Q].documents` 10, `t5[Q].one_key_term.named` 7, `t5[Q].one_key_term.unnamed` 7, `t5[_id.].documents` 10, `t5[_id.].no_key_term` 10, `t5[_id.].violation.key_without_prefix` 7 |
| `PUT /{index}/{path}` | 88 | shard: commit 86, find_key 88, get 67, metadata 50, replace 74, replace_by_key 69, set_metadata 50; `docview.data` 67, `docview.data@host` 67, `docview.value` 66, `docview.value@host` 66, `t5[Q].documents` 88, `t5[Q].one_key_term.named` 69, `t5[Q].one_key_term.unnamed` 74, `t5[_id.].documents` 88, `t5[_id.].no_key_term` 88, `t5[_id.].violation.key_without_prefix` 69 |
| `PUT /{index}/{path}/` | 5 | shard: find_key 5, get 5, metadata 3, reopen 4, replace 3, replace_by_key 3, set_metadata 2; `docview.data` 5, `docview.data@host` 5, `docview.value` 3, `docview.value@host` 3, `t5[Q].documents` 4, `t5[Q].one_key_term.named` 3, `t5[Q].one_key_term.unnamed` 3, `t5[_id.].documents` 4, `t5[_id.].no_key_term` 4, `t5[_id.].violation.key_without_prefix` 3 |
| `RESTORE /{index}/` | 2 | shard: commit 2, find_key 2, get 2, metadata 2, replace 2, replace_by_key 2, set_metadata 2; `docview.data` 2, `docview.data@host` 2, `docview.value` 2, `docview.value@host` 2, `t5[Q].documents` 2, `t5[Q].one_key_term.named` 2, `t5[Q].one_key_term.unnamed` 2, `t5[_id.].documents` 2, `t5[_id.].no_key_term` 2, `t5[_id.].violation.key_without_prefix` 2 |
| `RESTORE /{index}/{path}/` | 4 | shard: commit 3, find_key 3, get 2, metadata 1, replace 3, replace_by_key 1, set_metadata 1; `docview.data` 2, `docview.data@host` 2, `docview.value` 2, `docview.value@host` 2, `t5[Q].documents` 3, `t5[Q].one_key_term.named` 1, `t5[Q].one_key_term.unnamed` 3, `t5[_id.].documents` 3, `t5[_id.].no_key_term` 3, `t5[_id.].violation.key_without_prefix` 1 |
| `SEARCH /{index}/` | 82 | shard: commit 1, get 48; `callback.keymaker:Multi_MultiValueKeyMaker` 21, `callback.source:GeoSpatialRange.accept` 4, `callback.source:GeoSpatialRange.weight` 4, `callback.source:MultipleValueGE.accept` 2, `callback.source:MultipleValueGE.weight` 2, `callback.source:MultipleValueRange.accept` 3, `callback.source:MultipleValueRange.weight` 3, `callback.spy:AggregationMatchSpy` 74, `docview.data` 48, `docview.data@host` 48, `docview.terms` 1, `docview.terms@spy:AggregationMatchSpy` 1, `docview.value` 49, `docview.value@keymaker:Multi_MultiValueKeyMaker` 21, `docview.value@spy:AggregationMatchSpy` 28, `query.combine.binary` 7, `query.combine.nary` 11, `query.op.AND` 45, `query.op.AND_MAYBE` 1, `query.op.ELITE_SET` 2, `query.op.FILTER` 8, `query.op.NEAR` 1, `query.op.OR` 8, `query.op.PHRASE` 1, `query.text.default_op.AND` 1, `query.text.default_op.ELITE_SET` 1, `query.text.default_op.NEAR` 1, `query.text.default_op.PHRASE` 1, `query.text.flag.BOOLEAN` 1, `query.text.flag.FUZZY` 32, `query.text.flag.LOVEHATE` 32, `query.text.flag.PARTIAL` 2, `query.text.flag.PHRASE` 32, `query.text.flag.WILDCARD` 2, `query.text.partial_max_expansion` 32, `query.text.stem.none` 25, `query.text.stem.some` 7, `query.text.stem_language` 7, `query.text.stop` 7, `query.wildcard.combiner.SYNONYM` 1, `query.wildcard.limit.error` 1, `search.check_at_least` 77, `search.first` 1, `search.maxitems_zero` 29, `search.order.ascending` 80, `search.sorter.Multi_MultiValueKeyMaker` 21, `search.spy.AggregationMatchSpy` 77, `source.GeoSpatialRange` 4, `source.MultipleValueGE` 2, `source.MultipleValueRange` 3, `source.max_weight` 4 |
| `SEARCH /{index}/{path}/` | 14 | shard: get 13; `callback.keymaker:Multi_MultiValueKeyMaker` 3, `callback.source:MultipleValueRange.accept` 3, `callback.source:MultipleValueRange.weight` 3, `callback.spy:AggregationMatchSpy` 5, `docview.data` 13, `docview.data@host` 13, `docview.value` 3, `docview.value@keymaker:Multi_MultiValueKeyMaker` 3, `query.combine.binary` 4, `query.combine.nary` 6, `query.edit_distance.combiner.SYNONYM` 1, `query.edit_distance.distance.3` 1, `query.edit_distance.limit.error` 1, `query.edit_distance.prefix_length` 1, `query.op.AND` 14, `query.op.FILTER` 3, `query.op.OR` 7, `query.term.pos` 1, `query.text.flag.FUZZY` 7, `query.text.flag.LOVEHATE` 7, `query.text.flag.PARTIAL` 1, `query.text.flag.PHRASE` 7, `query.text.flag.WILDCARD` 2, `query.text.partial_max_expansion` 7, `query.text.stem.none` 5, `query.text.stem.some` 2, `query.text.stem_language` 2, `query.text.stop` 2, `query.wildcard.combiner.SYNONYM` 1, `query.wildcard.limit.most_frequent` 1, `query.wildcard.max_expansion` 1, `search.check_at_least` 5, `search.order.ascending` 14, `search.sorter.Multi_MultiValueKeyMaker` 3, `search.spy.AggregationMatchSpy` 5, `source.MultipleValueRange` 3 |
| `UPDATE /{index}/{path}` | 31 | shard: commit 31, find_key 31, get 31, replace 31; `docview.data` 31, `docview.data@host` 31, `docview.value` 31, `docview.value@host` 31, `t5[Q].documents` 31, `t5[Q].one_key_term.unnamed` 31, `t5[_id.].documents` 31, `t5[_id.].no_key_term` 31 |

## Facility totals

| Facility | Requests | Count |
|---|---|---|
| `callback.keymaker:Multi_MultiValueKeyMaker` | 24 | 6103 |
| `callback.source:GeoSpatialRange.accept` | 4 | 25 |
| `callback.source:GeoSpatialRange.weight` | 4 | 19 |
| `callback.source:MultipleValueGE.accept` | 2 | 26 |
| `callback.source:MultipleValueGE.weight` | 2 | 10 |
| `callback.source:MultipleValueRange.accept` | 6 | 787 |
| `callback.source:MultipleValueRange.weight` | 6 | 522 |
| `callback.spy:AggregationMatchSpy` | 79 | 39009 |
| `docview.data` | 262 | 1049 |
| `docview.data@host` | 262 | 1049 |
| `docview.did` | 2 | 5 |
| `docview.did@host` | 2 | 5 |
| `docview.term_details` | 27 | 27 |
| `docview.term_details@host` | 27 | 27 |
| `docview.terms` | 1 | 1000 |
| `docview.terms@spy:AggregationMatchSpy` | 1 | 1000 |
| `docview.value` | 191 | 34453 |
| `docview.value@host` | 139 | 237 |
| `docview.value@keymaker:Multi_MultiValueKeyMaker` | 24 | 6103 |
| `docview.value@spy:AggregationMatchSpy` | 28 | 28113 |
| `docview.values` | 27 | 27 |
| `docview.values@host` | 27 | 27 |
| `query.combine.binary` | 11 | 21 |
| `query.combine.nary` | 17 | 31 |
| `query.edit_distance.combiner.SYNONYM` | 1 | 1 |
| `query.edit_distance.distance.3` | 1 | 1 |
| `query.edit_distance.limit.error` | 1 | 1 |
| `query.edit_distance.prefix_length` | 1 | 1 |
| `query.kind.append` | 59 | 184 |
| `query.kind.combine` | 19 | 52 |
| `query.kind.edit_distance` | 1 | 1 |
| `query.kind.match_all` | 38 | 38 |
| `query.kind.source` | 10 | 12 |
| `query.kind.term` | 17 | 79 |
| `query.kind.text` | 39 | 61 |
| `query.kind.wildcard` | 2 | 2 |
| `query.nodes` | 95 | 429 |
| `query.op.AND` | 59 | 147 |
| `query.op.AND_MAYBE` | 1 | 4 |
| `query.op.ELITE_SET` | 2 | 32 |
| `query.op.FILTER` | 11 | 14 |
| `query.op.NEAR` | 1 | 1 |
| `query.op.OR` | 15 | 37 |
| `query.op.PHRASE` | 1 | 1 |
| `query.prepared` | 95 | 95 |
| `query.term.pos` | 1 | 1 |
| `query.text.default_op.AND` | 1 | 1 |
| `query.text.default_op.ELITE_SET` | 1 | 1 |
| `query.text.default_op.NEAR` | 1 | 1 |
| `query.text.default_op.PHRASE` | 1 | 1 |
| `query.text.flag.BOOLEAN` | 1 | 1 |
| `query.text.flag.FUZZY` | 39 | 61 |
| `query.text.flag.LOVEHATE` | 39 | 61 |
| `query.text.flag.PARTIAL` | 3 | 3 |
| `query.text.flag.PHRASE` | 39 | 61 |
| `query.text.flag.WILDCARD` | 4 | 4 |
| `query.text.partial_max_expansion` | 39 | 61 |
| `query.text.stem.none` | 30 | 38 |
| `query.text.stem.some` | 9 | 23 |
| `query.text.stem_language` | 9 | 23 |
| `query.text.stop` | 9 | 23 |
| `query.wildcard.combiner.SYNONYM` | 2 | 2 |
| `query.wildcard.limit.error` | 1 | 1 |
| `query.wildcard.limit.most_frequent` | 1 | 1 |
| `query.wildcard.max_expansion` | 1 | 1 |
| `search.calls` | 94 | 94 |
| `search.check_at_least` | 82 | 82 |
| `search.first` | 1 | 1 |
| `search.maxitems_zero` | 29 | 29 |
| `search.order.ascending` | 94 | 94 |
| `search.shards.5` | 94 | 94 |
| `search.sorter.Multi_MultiValueKeyMaker` | 24 | 24 |
| `search.spy.AggregationMatchSpy` | 82 | 82 |
| `shard.add` | 1 | 1 |
| `shard.commit` | 129 | 161 |
| `shard.erase` | 2 | 2 |
| `shard.erase_by_key` | 1 | 1 |
| `shard.find_key` | 228 | 16774 |
| `shard.get` | 261 | 1336 |
| `shard.metadata` | 62 | 425 |
| `shard.reopen` | 13 | 65 |
| `shard.replace` | 121 | 2152 |
| `shard.replace_by_key` | 85 | 471 |
| `shard.scan` | 2 | 10 |
| `shard.set_metadata` | 62 | 433 |
| `shard.stats` | 3 | 13 |
| `source.GeoSpatialRange` | 4 | 4 |
| `source.MultipleValueGE` | 2 | 2 |
| `source.MultipleValueRange` | 6 | 6 |
| `source.max_weight` | 4 | 4 |
| `t5[Q].documents` | 142 | 2624 |
| `t5[Q].one_key_term.named` | 86 | 472 |
| `t5[Q].one_key_term.unnamed` | 121 | 2152 |
| `t5[_id.].documents` | 142 | 2624 |
| `t5[_id.].no_key_term` | 142 | 2624 |
| `t5[_id.].violation.key_without_prefix` | 86 | 472 |
| `write.data` | 142 | 2624 |
| `write.documents` | 142 | 2624 |
| `write.named_key` | 86 | 472 |
| `write.terms` | 142 | 196326 |
| `write.terms.boolean` | 142 | 582 |
| `write.terms.positioned` | 142 | 113779 |
| `write.values` | 142 | 28910 |
