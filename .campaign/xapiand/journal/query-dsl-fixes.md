# query-dsl-fixes: journal

State: waiting on jobs 261109-185346-2edf6c 261109-185347-50ec8a 261109-185356-68cd98 261109-185357-8fe848
Quest: .campaign/xapiand/quests/query-dsl-fixes.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Code and tests are written and committed (tests-only commit 38f22b685, then the fix b56c33db7);
nothing has been compiled yet, so the first job result may be a compile error to fix.

## Next

1. Read the four jobs: tests alone (expect FAIL on both new tests), with the fix (expect pass), UBSan with the fix (pass), UBSan tests alone (expect a report or FAIL).
2. Fix any compile or test error, resubmit; then `campaign verify query-dsl-fixes` for build-test (A5).
3. Fill the Evidence table; set ready for review.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | pending: 261109-185347-50ec8a passes with the fix; 261109-185346-2edf6c fails without it |
| A2 | pending: same tests (test_range_aggregation_open_bounds); UBSan 261109-185356-68cd98 (fix) and 261109-185357-8fe848 (without) |
| A3 | pending: float and date cases in test_range_aggregation_open_bounds |
| A4 | served as the one-subquery form; the bare-object check in test_dsl_not_is_none_of |
| A5 | pending: `campaign verify` |

## Result

(pending)

## Rulings I made

1. `_not` is "none of": match_all AND_NOT (OR of the subqueries). Each element of the list is processed as under `_or`, so a map element with several fields means none of its fields (as `_or` reads it).
2. A4: a bare object is served (`{"_not": {...}}` and `{"field": {"_not": "v"}}` as the list of one), as the lead preferred. Before, it threw "Invalid operator: _not" (a 400); it never behaved like `_and`/`_or`, which have no bare-object form at the top level either.
3. Range bounds are `std::optional<T>` in the field's own type (uint64, int64, long double), one `configure<T>` replaces the three copies, one `aggregate_ranges<T>` the six loops. An open end is "not set", so a value equal to INT64_MAX is inside an upper-open range.
4. Beyond the quest text, same defect class, same function: `long double min()` is the smallest positive number, so an open lower bound on float and date fields dropped negative values (not only integers); now counted. Fractional bounds on integer fields round up (exact: `v >= 2.5` is `v >= 3`, `v < 2.5` is `v < 3`) instead of truncating, which also removes the float-to-int cast UB; a bound outside the type's values is open or empty by side (a negative `_from` on a positive field is open, a negative `_to` is empty). The default bucket key for an open end is unchanged ("..10", "0..", "..").
5. Bucket `slot` (used only to order buckets by key) of an open lower bound is -infinity, not the old tiny positive number.
6. Test: the DSL test takes each indexed term from the DSL's own output for `{field: value}`, so it does not depend on prefix details. Integer values at int64 extremes are INT64_MIN/MAX and +-2^62 only: the stored encoding goes through a double (sortable-encoding quest), so values near but not at powers of two would not round-trip.
