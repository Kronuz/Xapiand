# S3 (strata engine callbacks) code review

Reviewer: code reviewer, 2026-10-28. Subject: Xapiand:0ccd64652 (`git diff 0ccd64652^1 0ccd64652`), merged locally, not pushed.
Inputs read: `docs/status/strata-engine-callbacks.md`, `docs/mechanisms/strata-callbacks.md`, ruling R-P2 (strata `docs/design/xapiand-on-strata.md`), the diff of `metrics.cc`, `schema.cc`, `doc_view.*`, `shard.cc`, `search.cc` (callback parts), `tests/test_engine_callbacks.cc` `test_spy`, `harness/strata_callbacks_check.py`.
No GPL source read. `calibration.md` does not exist in this tree.

## Verdict: APPROVE WITH CONDITIONS

No revert needed. The values-based `_terms` is right (ground truth, 1 and 5 shards, both engines), the `did()` change has no consumer that breaks, the views have no lifetime hazard under a concurrent writer, and the term-list refusals are clean 400s. Two conditions before push/CI (CR1, CR2): a user-reachable 500 on strata where Xapian answers 400, and a harness that compares 500 with 500 on every sorted search.

## Build and proof rerun

ASan+UBSan, Apple clang, `-O1`, own dir and deps (`.scratch/s3-review/build`, deleted after), `quiet light`:

| Binary | Result |
|---|---|
| `xapiand_test_engine_callbacks` | 942 checks pass |
| `xapiand_test_engine_strata` | 9,568 pass |
| `xapiand_test_engine_search_strata` (`ulimit -s 65520`) | 231,298 pass |

The claimed counts match. UBSan reports only vendored lz4 (`lz4.c:549,1069`) and RapidJSON null-pointer offsets; no ASan report in any probe below.
Not rerun: `xapiand_test_engine_search` (4,423), `numeric_range_check.py` (797), `strata_callbacks_check.py` itself (its own gap is CR2).

## Findings

### CR1 (condition): errors raised inside a spy or key maker are 500 on strata, 400 on Xapian

`rethrow_translated` (`src/engine/strata/shard.cc:101-123`) maps any non-`engine::Error` `std::exception` to `errc::internal`. `search()` (`src/engine/strata/search.cc`, the `catch (...)`) applies it to exceptions thrown by `SpyView::see` and the sorter, which are Xapiand's own `AggregationError` / `SerialisationError` (client errors). S2 had no value-reading spy; S3 makes them reachable.

Probe `.scratch/s3-review/probe_errors.py` (5 shards, same request on both engines):

| Request | Xapian | Strata |
|---|---|---|
| `_sum` over keyword ("string type is not supported") | 400 | **500** |
| `_avg` over geo | 400 | **500** |
| `_avg` over boolean | 400 | **500** |
| `_terms` with unknown `_sort` field | 400 | **500** |
| `_terms` / `_values` over geo ("Bad encoded length") | 400 | **500** |

Same status on both for: histogram/range over keyword (400), `_terms` with no field (400), stats over text and `_sum` over date (200). The ledger claims "the refusals checked"; only the two R-P2 refusals are, and those are fine (see item 4).
Fix: let exceptions from the callbacks reach the caller unchanged (capture an `exception_ptr` in the `key`/`visit` lambdas and rethrow it after `p::search`, or skip translation for non-strata exceptions), plus a test that a spy throwing a client error keeps its type. The existing test (`test_spy`, "A spy that throws fails the search") only checks `has_value()`.

### CR2 (condition): every "sorted" comparison in `strata_callbacks_check.py` is a 500 on both engines

`probe_sortshape.py`: `{"_sort": {"balance": "desc"}}` and `{"_sort": [{"age": "asc"}, {"balance": "desc"}]}`, the two shapes the harness uses (`queries()`: 12 searches `sorted / ...` and `sorted desc / ...`), return `500 "STR is not iterable"` on both engines. The harness compares `res["xapian"] != res["strata"]` including the status, so 500 equals 500 and the "0 different" over those 12 proves nothing about the key maker through the server. The working shapes are `{"balance": {"_order": "desc"}}` and `["age"]`.
With working shapes (`probe_sortcmp.py`, 400 docs, 1 and 5 shards): total-order sorts (age asc, balance desc) and balance desc with aggregations are identical on both engines, hits and aggregations. Sorts with ties in key and weight (age alone, state alone) differ in the order inside tie groups (same keys in the same order, e.g. `d9,d6` vs `d6,d9`). I could not tell whether that is the key maker or the two engines assigning different docids to the same RESTORE batch, because `:info` (which shows the docid) is refused on strata; the unit test `test_sorter` agrees on identical docids. Treat as unverified.
The `{"balance":"desc"}` 500 itself is not S3's (it is the same on Xapian) but it is a bug in a documented shape; file separately.
Fix: change the harness's sort shapes, and assert `status == 200` for every compared search so an error pair cannot pass.

### Item 1: `_terms` over values (no blocker)

Probes `probe_terms.py`, `probe_truth.py` (outputs `probe_terms.out`, `probe_truth*.out`), 300 docs, 1 and 5 shards, both engines:
- Ground truth computed in Python from the loaded documents (distinct values per document counted once): `ns` (integer array with repeats), `f` (float), `flag`, `kw` (keyword array with `Red`/`red`, repeated values, a 100-char value, empty array, null), `code` (`_index: field_values`), `u` (unique keyword): 0 mismatches on all, all four combinations. Keys are values (`"1e+10"`, `"-7.125"`, `"true"`, `"1578650400"` for a date), not encoded bytes. Case kept (`Red` and `red` two buckets) on both engines, as documented.
- 1 vs 5 shards on strata: identical for every field.
- Missing field: no buckets, 200, both engines. Missing values in a field present on some documents: counted only where present.
- A keyword of 400 characters drops the whole document at RESTORE on both engines (pre-existing, silent); 100 characters round-trips.
- Date `_terms` keys are epoch seconds, not ISO strings; same on both engines and same as `_values`. Not a regression, but the changelog row "values only" does not say it.
- Geo: `_terms` and `_values` fail with "Bad encoded length" on both engines. `probe_geo.py` shows the Xapian engine also fails through the old terms path (`_index: field_terms`), so this is not introduced here; the strata status is CR1.
- `_index: field_terms` numeric: Xapian still returns the accuracy/range terms; strata refuses (400). Documented.
- Dynamic string field (`{"s": "hello world"}`): Xapian returns words, strata 400. Documented; the message says "count a keyword field" which is the right pointer.
- Cardinality (`probe_truth.py`, `CARD=200000`, 5 shards, ASan build): `_terms` over 200,000 distinct keywords and 200,000 distinct integers returns in about 2 s with a resident growth of about 100 MB per request on both engines (about 500 bytes per bucket under ASan). Nothing bounds it: `BucketAggregation` keeps a `std::map` bucket per distinct value per shard (`bucket.h` `serialise_results` / `merge_results`) and applies `_limit` only on output. The behaviour is unchanged by S3 for keyword and text, but it now also applies to numeric and date fields, which previously yielded a few accuracy terms per document. A design finding, not a blocker: a `_terms` over a high-cardinality field on a many-index host is O(distinct values) in memory per request and per merge; the architect may want a shard-size cap (Elasticsearch's `shard_size`).
- Code: `TermsHandler` and `ValuesHandler` are now the same read for every non-text field (`metrics.cc:96-141`); a later cleanup could fold them.
- `_from_values` is decided from the field's own `_index` (`schema.cc` `get_slot_field`); a field with no schema entry keeps `field_values = true`, then reads the slot (a miss), matching the "no buckets" result above.

### Item 2: `did()` is the id within the shard (no blocker)

`prospect::match::did` is the shard-local id and `docid` the interleaved one (`shards.hh`, pinned becfe3c); `search.cc` passes `m.did`. Consumers of `DocView::did()` outside the engines: only `DatabaseHandler::dump_documents` (both overloads, `handler.cc:799,841`), which reads it from `Shard::scan` and already treats it as shard-local (`(shard_did - 1) * n_shards + shard_num + 1`). No aggregation, sort key maker or dedupe reads `did()` (grep over `src/`). `test_spy` compares the multiset of ids with Xapian's on 1 and 5 shards. Server level, 5 shards: aggregations and total-order sorts equal Xapian's (item above). Gap: `test_spy`'s corpus has no deletions or replacements, so a document whose shard-local id is not its ordinal is not exercised for `did()` (the dump path does exercise ids via `scan`).

### Item 3: `SegmentDocView` lifetime and concurrency (no blocker)

The view borrows a `const prospect::segment*`, is built per match on the stack and passed by const reference. Spies in Xapiand (`AggregationMatchSpy`) and `Multi_MultiValueKeyMaker` use it synchronously. The search holds the shard's `Pinned` (shard mutex plus the `segment_list`) for its whole run, so a refresh or merge cannot free a segment under a view; a spy that kept the view past `see()` would be the spy's bug, and no Xapiand spy does.
Probe `probe_concurrent.py` (ASan+UBSan strata server, 5 shards, 3 writers doing commit/replace/delete, 4 readers running `_terms` aggregations, sorted searches and sorted searches with aggregations, 60 s): 0 ASan reports, 0 UBSan reports outside vendored lz4; 272 reads ok, 11 reads got `404 Document not found`, which is the existing race between a search and a delete before the hit's document is fetched (not the view). Residual: the reads/s are low under ASan and 3 busy writers; this proves no obvious race, not freedom from one. TSan not run (not asked).
Perf note (not measured, no benchmarks run): `SegmentDocView::data()`/`value()` re-read the stored block / construct `values::Reader` on every call and cache nothing, so a spy with N sub-aggregations reading `data()` pays N reads per match. Worth a number from the benchmark owner before the many-index workload.

### Item 4: refusals (no blocker)

`probe_terms.py`/`probe_truth.py` over HTTP, strata: `_terms` over `text` 400, `_terms` over a dynamic string field 400, `_terms` nested under a bucket aggregation 400, `_terms` over a `field_terms` field 400, `INFO` (and `?human`) on a document 400, all with "keeps no term lists... count a keyword field's stored values". Same requests on Xapian: `_terms` text and dynamic string answer 200 with words, `INFO` answers 200 with the terms. `DocView::terms`/`term_details`/`serialise` raise `errc::syntax` in both `SegmentDocView` and `OwnedDocView`; `test_spy`'s `TermsSpy` covers all three calls in-process. `Document::hash()` (`handler.cc:2654`, `term_details`) and `Document::serialise()` (`handler.cc:2342`) are also refused now (before S3, `errc::not_available`); I found no HTTP route to them, so I did not exercise them. If a replication or checksum path reaches them on a strata index it fails with the same 400; the architect should confirm none does.

## Other notes (non-blocking)

1. Changelog and mechanism note list numeric/date/keyword/boolean/text rows; add geo (fails on both engines, pre-existing) and the date key format (epoch seconds).
2. `docs/status` "Proof" cites mutants for the `_terms` values path caught by "the harness (62 searches differ)"; given CR2 the harness is weaker than stated for sorted searches, not for `_terms` (truth probe agrees).
3. The harness's `_terms` agreement across engines is partly self-referential (both engines now run the same values code); the real check is the in-harness bucket-sum assertion and the ground truth above.

## Probes (all in `~/code/Kronuz/Xapiand-wt/strata-callbacks/.scratch/s3-review/`)

`build.sh`, `probe_terms.py` (+ `.out`), `probe_truth.py` (+ `probe_truth{,2,3}.out`), `probe_errors.py`, `probe_sort.py`, `probe_sortshape.py`, `probe_sortcmp.py`, `probe_concurrent.py`, `probe_str.py`, `probe_geo.py`. Run as `python3 probe_X.py <xapiand binary>`; they start two servers on ports 19100-19800 with `XAPIAND_ENGINE`. Build dir and server data directories deleted.

## Conditions

1. CR1: spy and key-maker exceptions keep their type (400 for client errors), with a test that fails when the translation is restored.
2. CR2: harness asserts 200 and uses a working `_sort` shape.

Not run: `xapiand_test_engine_search` and the other Xapian-engine tests, `numeric_range_check.py`, `strata_callbacks_check.py`, the fuzz target `fuzz_strata_pattern`, TSan, Linux/FreeBSD, any benchmark, a mutant sweep, the architect-level question of unbounded `_terms` memory (design finding only).
