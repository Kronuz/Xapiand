# nan-range-bound: journal

State: waiting on jobs 261123-000908-5ce25f 261123-000920-aac63a 261123-000923-5c3bde 261123-000927-324fb3 261122-213854-8ab1db
Quest: .campaign/xapiand/quests/nan-range-bound.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Code round done at Xapiand:ca144f228 (pushed). Jobs queued, ids below.

## Next

1. Read each log: a complete build, then for the base (1dffb8ab8) both tests failing on f and d
   (aggregation) and d plus every message (query); the float mutant (56ca1b70f) failing only
   `test_range_aggregation_nan_bound` on f and d; the query mutant (6a9bd08a4) failing
   `test_query_range_nan_bound`. Then CI at ca144f228 for A4.

## Blocked on

Nothing (msg from the lead widened the file set to src/query_dsl.cc,
src/aggregations/bucket_numeric.h and the two doc pages; done below).

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261123-000908-5ce25f (test_nan_range + test_engine_query at ca144f228); fails-without 261123-000920-aac63a (head's test over main's src, 1dffb8ab8, ref mutant/nan-range-bound-base); float mutant 261123-000923-5c3bde (56ca1b70f, ref mutant/nan-range-bound-float); query mutant 261123-000927-324fb3 (6a9bd08a4, ref mutant/nan-range-bound-query): pending |
| A2 | 261122-213854-8ab1db (suite at ca144f228): pending |
| A3 | range-searches.md and range-aggregation.md under docs/src/content/docs/reference-guide/search/ say a NaN bound answers 400 naming the field and the bound |
| A4 | pushed at ca144f228; CI not read yet |

Superseded, cancelled: 261122-213206-6de57d, 261122-213210-593d2f, 261122-213213-6018a2,
261122-213223-ea3b17, 261122-213233-e1b2f5 (all at older code, never ran).

Precheck (lead's note): `grep -rniE 'nan|isnan|quiet_NaN' docs/ tests/` finds no example, doc or
test relying on NaN matching nothing. The docs already said a NaN bound answers 400
(docs/mechanisms/value-encoding.md:38, upgrade.md:69, changelog.md:50); tests touching NaN
bounds expect refusal (test_coverage_multivalue_range.cc:269, numeric_range_check.py:405).

## Result

A NaN `_from`/`_to` answers 400 "Field 'f' range bound '_from' cannot be NaN" on every field
type, in a query's `_range` and in a range aggregation. Before: aggregations answered "bounds
must be numbers" on integer/positive and silently matched nothing on floating and date types;
queries answered Serialize::exact's message on numeric fields and "Year is out of range" on
dates. Two checks, one per entry point, same message: the bound reader in
src/aggregations/bucket_numeric.h and `exact_range` in src/query_dsl.cc. Tests:
tests/test_nan_range.cc (4 field types x 2 bounds x 2 forms, status and message, plus a finite
bound answering 200).

## Rulings I made

- The aggregation and query messages name the field and the bound name (`_from`/`_to`).
- The float mutant skips the aggregation check only for floating-point T (the old floating and
  date behavior); the query mutant removes the `exact_range` check.
