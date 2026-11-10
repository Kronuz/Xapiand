# query-dsl-fixes: journal

State: waiting on jobs 261110-030126-7ce5bf 261110-030126-f64c97 261110-030126-19d97e 261110-030127-46d106 (rebased onto master; reviewer re-reviewing D50 in bucket.h)
Quest: .campaign/xapiand/quests/query-dsl-fixes.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Rebased onto master (head ace371929, code same as Xapiand:c73e3c7f1: F4, F3, A6 / D50 in bucket.h). The
reviewer is re-reviewing the D50 change; I do not touch code. Evidence jobs rerun at the head.

## Next

1. When the four jobs below pass: set State "ready for review".
2. If a job fails on something other than these tests, show it also fails at main and report.

## Blocked on

Nothing.

## Evidence

Jobs at the head (Xapiand:ace371929): test_engine_query with the fix, ASSERTS=ON 261110-030126-19d97e;
under UBSan with float-cast-overflow 261110-030126-f64c97; reviewer part-A probe 261110-030126-7ce5bf;
build-test 261110-030127-46d106.

Expected failures, at other commits on purpose (listed here, not in the rows): without the fix, tests
alone (38f22b685) 261109-185346-2edf6c (range test: A2, A3), under UBSan 261109-185357-8fe848; the
probe's part A at master's query_dsl.cc 261109-195413-671cc0 (A1, A4; its rerun at the head is the part-A
job above); A6 job 261110-022727-1dd71e (reviewed f176d8896 plus the head's test file; fails only on the
two default-key checks, :1270, :1280); F3 mutant 261110-022803-ef3749 (head with open lower slot 0; fails
only on the two key-order checks, :1309, :1310). Superseded: 261110-021740-4105e9 and 261110-021741-fbcbbf
(my misplaced `_sort` in the key-order test), 261109-185347-50ec8a, 261109-185356-68cd98 (test setup bug,
Rulings 7), 261109-192344-c4ac9a, 261109-192345-1c7534 (bound-tag bug, Rulings 8), the earlier passes at
c73e3c7f1 (261110-022726-fafa29, -f81866, 261110-022824-c5a2f4, 261110-024407-b289b5).

| Criterion | Evidence |
|---|---|
| A1 | `test_dsl_not_is_none_of` passes with the fix 261110-030126-19d97e; the same checks (`_not` and the audit cases) fail at master's query_dsl.cc in part A of the probe (expected failures above), whose rerun at the head is 261110-030126-7ce5bf |
| A2 | `test_range_aggregation_open_bounds` passes with the fix 261110-030126-19d97e and under UBSan 261110-030126-f64c97; it fails without the fix (expected failures above); the key-order check (Rulings 5) is killed by the F3 mutant (above) |
| A3 | float and date cases in the same test, 261110-030126-19d97e. Note: negative float/date values with an open lower bound were dropped at master (min() is positive); the test counts them |
| A4 | served as the one-subquery form (Rulings 2); bare-object checks in `test_dsl_not_is_none_of`, 261110-030126-19d97e; they fail at master in the probe's part A (above), rerun 261110-030126-7ce5bf |
| A5 | build-test at the head 261110-030127-46d106; CI after landing |
| A6 | default keys `2.5..`/`3..` (integer) and `-5..4`/`..4` (positive), each with its own count, in `test_range_aggregation_open_bounds`: passes 261110-030126-19d97e; fails at the reviewed commit f176d8896 with the new test (A6 job above) |

## Result

(pending)

## Rulings I made

1. `_not` is "none of": match_all AND_NOT (OR of the subqueries). Each element of the list is processed as under `_or`, so a map element with several fields means none of its fields (as `_or` reads it).
2. A4: a bare object is served (`{"_not": {...}}` and `{"field": {"_not": "v"}}` as the list of one), as the lead preferred. Before, it threw "Invalid operator: _not" (a 400); it never behaved like `_and`/`_or`, which have no bare-object form at the top level either.
3. Range bounds are `std::optional<T>` in the field's own type (uint64, int64, long double), one `configure<T>` replaces the three copies, one `aggregate_ranges<T>` the six loops. An open end is "not set", so a value equal to INT64_MAX is inside an upper-open range.
4. Beyond the quest text, same defect class, same function: `long double min()` is the smallest positive number, so an open lower bound on float and date fields dropped negative values (not only integers); now counted. Fractional bounds on integer fields round up (exact: `v >= 2.5` is `v >= 3`, `v < 2.5` is `v < 3`) instead of truncating, which also removes the float-to-int cast UB; a bound outside the type's values is open or empty by side (a negative `_from` on a positive field is open, a negative `_to` is empty). The default bucket key for an open end is unchanged ("..10", "0..", "..").
5. Bucket `slot` (used only to order buckets by key) of an open lower bound is -infinity, not the old tiny positive number.
6. Test: the DSL test takes each indexed term from the DSL's own output for `{field: value}`, so it does not depend on prefix details. Integer values at int64 extremes are INT64_MIN/MAX and +-2^62 only: the stored encoding goes through a double (sortable-encoding quest), so values near but not at powers of two would not round-trip.
7. The DSL returns an append node (empty query plus the term) for a field clause; the test unwraps it. The first with-fix failure ("index rejected the operation") was this test setup, not the fix.
8. A signed int bound that is not negative can carry the negative-integer tag, so `reach_of` reads the value, not the tag (found by the positive-field case).
9. F2 (owner, D50): the default key is the request's bounds as written (integers as integers, floats as doubles in shortest form), ".." with no bounds. A named `_key` is unchanged. Float keys for a bound written as a double now print its shortest form (`0.1`), not the long double's digits.
