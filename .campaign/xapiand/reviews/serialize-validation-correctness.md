# serialize-validation review: correctness

Commit: Xapiand:7f03ae08d3b5
Verdict: CHANGES
Date: 2026-11-19

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- Suite 261119-142617-01e205 and test job 41ab85 (the seven quest tests plus the flipped coverage pins) pass at
  Xapiand:72be2396c; 72be2396c..28bfd2e27 changes only the journal, so they stand for this commit.
- Fails-without jobs 76dfe9 (A1), 159564 (A2), 7f59ed and b9fd21 (A4), 222d40 (A3), e9f90f, 367e06 and 859ec0 (A7):
  the criteria's unit-level claims fail without their fixes. Not rerun.
- The two earlier review probes, never written up: 261119-002707-c3d713 failed (exit 2) in its ASan+UBSan build step,
  on diagnostics from `.deps/http_lib-src/http_asio_tls.h` and asio headers, files outside this quest's diff;
  261119-003619-41690e reached quiet's 30-minute deadline in the same build step (exit 124). Neither ran a test or the
  HTTP probe, so neither points at a defect here. Their HTTP probe (`http_probe.py`) runs in aed66c below, unsanitized.

## What I checked myself

Scripts under `.scratch/xapiand/review-serialize-validation-correctness/` in the home checkout; `run_probe.sh build`
builds `xapiand` in the quest slot's `build/`, then `run_probe.sh run` or `run_probe3.sh` drives a fresh node.

- 261119-150614-aed66c (`http_probe2.py`, then `http_probe.py`): every refusal over HTTP is a 400 naming the value,
  and leaves no document and no new schema field (PUT, UPDATE, UPSERT, PATCH, nested object, `_namespace`, a refused
  `_type`, a refused cast on a new field). RESTORE answers a per-item 400 for "-1" and "-0x1", writes the good items and
  none of the bad. No document holds a wrapped U64_MAX. Range bounds and sort values of "-1" on a positive field are
  400. Geo exact queries answer exactly the documents at each point (field, EWKT); `_in` and distance sort unchanged.
  Unknown aggregation types at the top level, after `_aggs`, and nested under a bucket with matching documents are
  400 naming the type. (The probe's FAIL lines on 204 statuses, "+7"/"+5", untyped strings guessed as text and the
  degenerate polygons are the probe's own wrong expectations, not defects: "+" is refused by the exact rule on master.)
- 261119-150832-e16f70 (`http_probe3.py`): whether the D130 and A4 casts reach HTTP at this commit (S1, S2, S5).
- 261119-151123-15e6a5: the same `http_probe3.py` at the quest's merged master (Xapiand:290336d04), for attribution:
  every line answers as at this commit, so S1, S2, S5 and S8 are not introduced here.
- `phf_audit_noxapian.py` over `src/` (excluding `src/xapian`): no phf case label outside its table at this commit;
  the same audit on master finds the three this quest fixes (aggregations.cc:217, schema.cc:925, serialize.cc:1694).
- Reading: every `strtoull`/`strict_stoull` call site outside `src/xapian`; `Cast::cast(FieldType::empty)`,
  `Cast::positive`, `positive_of`, `number_of`, `exact_number`; the `guess_serialize` map branch against the switch it
  replaced (same bytes for every cast but `_time`/`_timedelta`, which now get time bytes, and `_date`, now served);
  the indexer's geo term and value slot (the hash reaches only the exact-match term, as the note says).

Cast edges, by reading `src/cast.cc` at this commit: `Cast::cast(empty)` reads "-9223372036854775809" and
"-18446744073709551615" as floats (no wrap), "18446744073709551616" as a float, "-0" as the integer 0, "1e-5" as a
float; `Cast::positive` refuses a leading '-' and white space and range-checks floats to [0, 2^64). No wrap is left on
the serialize, cast or schema value paths.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| S1 | yes | gap | A5 records `_date` as served (D130), but no HTTP path serves it: a value `{"f": {"_date": "2020-01-02"}}` is a 400 "Field '_date' in 'f' is not valid", and a field or global query `{"_date": ...}` is a 400 "Invalid operator: _date". The schema's property tables and dispatch hold `RESERVED_DATETIME` but not `RESERVED_DATE` (src/database/schema.cc:6475, 6551, 6667), nor do the query DSL's operator tables (src/query_dsl.cc:270, 380, 1404, 1449). Only the Cast and Serialize layers serve it, which the unit test covers. Master answers the same (15e6a5): nothing regressed, but the criterion's outcome is not reached. | e16f70, 15e6a5; schema.cc:6667; query_dsl.cc:270 | Either (a) add `RESERVED_DATE` beside `RESERVED_DATETIME` in schema.cc's two tables and dispatch and in query_dsl.cc's two operator tables (query_dsl.cc is outside the file set: the lead widens it), with an HTTP test that indexes, queries, ranges and sorts a `_date` value (it will meet S8); or (b) the lead narrows D130 to the Cast layer and files the HTTP half. |
| S2 | no | gap | The `_time` and `_timedelta` casts never reach `guess_serialize` from HTTP: values are "Field '_time' in 'f' is not valid" and queries "Invalid operator: _time" (same tables as S1). A4's fix is right but reaches users only once those tables hold the words. Not introduced here. | e16f70 | For the lead: the same table entries as S1, or a hunt row. |
| S3 | no | defect | An empty aggregation `{"x": {}}` answers 500 "The map only contains 0 elements": the type loop dereferences `begin()` of an empty map (src/aggregations/aggregations.cc:124). An aggregation holding only a comment or only `_aggs` is dropped silently, and a key after the type is never read (`{"_count": ..., "_bogus": 1}` answers 200). All pre-existing lines. | aed66c | Fold into agg-validate-tree: refuse an aggregation with no type, or a second type, with a 400. |
| S4 | no | nit | Two rules for "-0" on a positive: `Serialize::positive("-0")` refuses it (the ruling and the test), while the HTTP write and query paths read it through the exact rule as 0 (PUT 204, a query for "-0" finds it). HTTP reaches `Serialize::positive(string_view)` only through id generation. The no-'-' rule is also spelled three times (serialize.cc:210, cast.cc:138, cast.cc:226). Reading "-0" as 0 is correct, so nothing is wrong today. | aed66c; serialize.cc:210 | Keep both; or have `Serialize::positive(string_view)` read through `number_of`/`exact_number`, one rule for every path. Owner's call. |
| S5 | no | nit | A `_lua` value over HTTP is a 400 "Type: 0x58 is an unknown type", not D130's "'_lua' is a script, not a value": the schema types the field script through `Cast::get_field_type` and fails in `Serialize::serialize`'s default. No state changes. Not introduced here. | e16f70 | Refuse a script-typed value with the D130 message in the schema's value path, or leave it. |
| S6 | no | gap | src/database/schema.cc:1602 and 1611 (`strict_stoull` on accuracy suffixes) are in this quest's file set, though the journal lists them as outside it; a field name such as `_-1` reads accuracy U64_MAX. Low impact: a query for a nonexistent accuracy. | schema.cc:1602 | For the lead: a hunt row or fold into a later serialize quest. |
| S7 | no | gap | The exact geo query hashes ranges at `DEFAULT_GEO_PARTIALS` and `DEFAULT_GEO_ERROR` (serialize.cc:963, 971) while the indexer uses the field's `_partials` and `_error` (schema.cc:5579), so an exact query for a non-point geometry on a field with a non-default `_error` matches nothing. Pre-existing; docs/mechanisms/geo-exact-term.md describes only the indexer side. | serialize.cc:963 | A sentence in the note; the fix belongs to geo-correctness. |
| S8 | no | defect | Outside this quest, found on the way: a new field given `{"_datetime": "2020-01-02"}` over HTTP is a 400 "Error format in _time: 2020-01-02" (src/datetime_msgpack.cc:270), at this commit and on master alike; a datetime cast of a plain date is refused through the time parser. | e16f70, 15e6a5 | For the lead: a hunt row (the datetime cast path in the schema). |

Aggregations and agg-validate-tree: the phf now holds `_aggs` and `_aggregations`, so every miss reaches the refusal;
a nested unknown type under a bucket with matching documents is a 400 naming it, and with no matching documents a 200
(agg-validate-tree's A1). agg-validate-tree should validate through this same type switch (built eagerly, or the
dispatch moved into one function both call), never a copied table, and take S3 with it.

## Not run

- Sanitizers: c3d713 and 41690e never got past the build; CI's ASan and TSan rows are the sanitizer evidence.
- The `_multiconvex` map form (geo-correctness's) and multi-node or remote-shard aggregation (a nested refusal
  raised on a remote shard).
- A rerun of the unit tests or the fails-without jobs.

## Re-check

Commit: Xapiand:16e833a04
Verdict: APPROVE
Date: 2026-11-19

S1 is closed on the value path. The fix (Xapiand:e5114e3a3, Xapiand:645a41db7) adds `RESERVED_DATE` to the schema's two
property tables and dispatch. It also makes `Schema::store_item` read a cast object through `Cast::cast` for
`FieldType::date` and `FieldType::datetime` only. Every other type keeps the default branch, which stores the value as
given. The query half (`{"_date": ...}` as an operator) stays with dsl-date-operators, which is filed.

Evidence relied on: the S1 HTTP test 4fbe57, the suite ff5706 and the mutant eb6c56, as the lead reports them. Not rerun.

What I checked:

- 261119-155820-b92bea (at Xapiand:16e833a04) and 261119-155825-2bdf03 (at Xapiand:28bfd2e27, before the fix) ran the
  same `http_probe4.py`. It covers 25 typed values, each on a fresh index: PUT, GET, the schema type, a term query on
  the value and, where one applies, a range.
- uuid, `_point`, `_ewkt`, `_positive`, `_integer`, `_float`, `_boolean` and `_keyword` casts answer byte for byte
  alike at both commits: the same GET body (the cast object as sent), the same type and the same query hits. No stored
  form changes outside date and datetime. `_time` and `_timedelta` still never reach `store_item` (S2), so they are
  unchanged too.
- The datetime casts that the fix moves only from a refusal to served:
  - `{"_datetime": "<datetime>"}`, with a timezone, date-only (S8), as a number, untyped, and inside an array: each was a
    400 "Error format in _time" or "'_datetime' must be a map value". Each now stores an ISO 8601 text, GET returns
    it, and the term and range queries find the document.
  - `{"_float": 1577961000.5}` into a datetime field was a 500 "Unsupported Datetime: '_float'". It now stores
    "2020-01-02T10:30:00.5".
  - `{"_date": ...}` on a new field is now type date and served; into a datetime field it now stores the date at
    midnight.
- The parts form `{"_datetime": {"_year": ...}}` and plain datetime texts and numbers store the same text at both
  commits.
- No double cast. `index_item_value` (schema.cc:3549-3551) hands the same `item_value` to `index_item` and then to
  `store_item`, and each casts it once. The indexed bytes and the stored text come from the same `Cast::cast` result,
  so what is stored is what is indexed. The probe's term and range hits on every served datetime case confirm it.

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| R1 | no | gap | `{"_datetime": {"_date": "2020-01-02", "_time": "10:30:00"}}` is a 400 "'_date' must be a map value" at both commits: the datetime map parser takes `_date` only as a parts map. This is pre-existing and does not change here. | b92bea, 2bdf03 | For the lead: a hunt row, or dsl-date-operators' neighbour. |
| R2 | no | nit | S8 is fixed as a side effect of this commit. No test asserts it: the HTTP test covers `_date` only. | b92bea | Add `{"_datetime": "2020-01-02"}` and the 500 case `{"_float": ...}` into a datetime field to tests/test_serialize_validation_date_http.py. |

The scripts are `http_probe4.py` and `run_probe4.sh` in `.scratch/xapiand/review-serialize-validation-correctness/`.

## Re-check (7f03ae08d3b5)

Commit: Xapiand:7f03ae08d3b5
Verdict: APPROVE
Date: 2026-11-20

Only tests/test_coverage_core_cast.cc changed since the approval (Xapiand:c7db5eeeb); the rest are master merges. Each flipped pin matches src/cast.cc at the merge head:

- `get_field_type("_date")` is `FieldType::date` (cast.cc:438, the `HashType::DATE` row).
- `_multiconvex` is `FieldType::geo` (cast.cc:445), and `Cast::cast` returns the map whole (the geo group returns `obj`, cast.cc:77-87). The new pin compares the whole map's text, so it asserts more than the old throw.
- `_lua` throws "is a script, not a value" (cast.cc:83-84); `get_field_type("_lua")` stays `script`.

No pin asserts less. Each old line asserted a refusal; each new line asserts the exact type or value. Suite 261120-001940-4ad3ed passed at c7db5eeeb; suite 261120-012601-a2aa5d at the merge head was queued when this was written.

No findings.
