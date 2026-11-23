# nan-range-bound: journal

State: waiting on jobs 261122-213206-6de57d 261122-213213-6018a2 261122-213223-ea3b17 261122-213233-e1b2f5; blocked on lead: date-query field naming (see Blocked on)
Quest: .campaign/xapiand/quests/nan-range-bound.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Fix and test committed (Xapiand:c7c964a16); jobs queued: A1 at head 261122-213206-6de57d,
fails-without at the test-only commit 93919edb5 (main's src) 261122-213223-ea3b17, float mutant
78d6c4b6b (ref mutant/nan-range-bound-float) 261122-213213-6018a2, suite 261122-213233-e1b2f5.

## Next

1. Read the jobs' logs: the fails-without log must show a complete build and
   `test_range_aggregation_nan_bound` failing on f and d only; the mutant likewise.
2. On the lead's answer to the date-query item below: name the field there too and add the `d`
   checks to `test_query_range_nan_bound`.

## Blocked on

lead: a NaN bound in a date field's query `_range` answers 400 "Year is out of range"
(DatetimeError, caught by catch_http_errors), which names neither the field nor the bound. The
field name never reaches src/multivalue/range.cc (`MultipleValueRange::getQuery` takes only the
field spec); it is known in `exact_range` in src/query_dsl.cc, outside the file set. Options:
1. Widen the file set to src/query_dsl.cc: `exact_range` refuses a NaN bound for every field type
   before the type's own rule, one place naming field and bound (recommended).
2. Pass the path into `MultipleValueRange::getQuery` (same call site, still query_dsl.cc).
3. Accept the date query as it is: already a 400, without the field named.
Integer, positive and floating queries already answer 400 "Field 'f' expects a value of type
floating, got nan" (Serialize::exact); no change there.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261122-213206-6de57d (head), 261122-213223-ea3b17 (fails-without at 93919edb5), 261122-213213-6018a2 (float mutant): pending |
| A2 | 261122-213233-e1b2f5 (suite at c7c964a16): pending |
| A3 | docs/src/content/docs/reference-guide/search/query-dsl/compound-queries/range-searches.md and .../aggregations/bucket/range-aggregation.md say a NaN bound answers 400 naming the field |
| A4 | not yet pushed |

Precheck (lead's note): `grep -rniE 'nan|isnan|quiet_NaN' docs/ tests/` finds no example, doc or
test relying on NaN matching nothing. The docs already say a NaN bound answers 400
(docs/mechanisms/value-encoding.md:38, upgrade.md:69, changelog.md:50); tests that touch NaN
bounds expect refusal (test_coverage_multivalue_range.cc:269, numeric_range_check.py:405).

## Result

A NaN `_from`/`_to` in a range aggregation answers 400 "Field 'f' range bound '_from' cannot be
NaN" on every field type (was: 400 "bounds must be numbers" on integer/positive, silently no
match on floating/date/datetime/time/timedelta). One check in the bound reader of
src/aggregations/bucket_numeric.h, before any type's reading. Tests: tests/test_nan_range.cc.

## Rulings I made

- The file set names src/aggregations/bucket.h, now only the umbrella include; the range code is
  in src/aggregations/bucket_numeric.h, which I edited. Likewise the docs glob
  `docs/src/content/docs/reference/query-dsl*` matches nothing; the reference pages are under
  reference-guide/search/ (range-searches.md, range-aggregation.md). Lead: widen the set.
- The aggregation message names the field and the bound name (`_from`/`_to`); the query side keeps
  Serialize::exact's message, which names the field and the bound value.
- The mutant skips the NaN check only for floating-point T: the old floating and date behavior.
