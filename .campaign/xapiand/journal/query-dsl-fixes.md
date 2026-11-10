# query-dsl-fixes: journal

State: waiting on jobs 261109-193622-6d23b4 261109-193623-40183f (261109-185357-8fe848 may still be queued)
Quest: .campaign/xapiand/quests/query-dsl-fixes.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Reruns at a8ca32c7f (261109-192344-c4ac9a, UBSan 261109-192345-1c7534): `_not` test passes; one
check failed: positive field, `{_from:-5,_to:4}` gave no bucket (printed: only "below"=5). Cause: the
fix, not the expectation. reach_of took the NEGATIVE_INTEGER tag as "below every value" on unsigned
fields; the int literal 4 evidently carries that tag, so the range was marked empty. Fixed in
ffc7e9517 by reading the value (i < 0) instead of the tag. Reruns: 261109-193622-6d23b4 (test),
261109-193623-40183f (UBSan).

## Next

1. Read the reruns; both must pass. Then `campaign verify query-dsl-fixes` (A5), and fill the Evidence
   table (tests alone fail: 261109-185346-2edf6c; UBSan tests alone: 261109-185357-8fe848).
2. The `positive bucket` debug print in the test can stay or go; remove it once green.

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
