# query-dsl-fixes: journal

State: waiting on jobs 261110-021740-4105e9 261110-021740-d286d4 261110-021741-fbcbbf (+ build-test verify)
Quest: .campaign/xapiand/quests/query-dsl-fixes.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Rebased onto master (code at 9ea2fc771: review F4 print removed, F3 key-order test added). Reviewer
approved f176d8896 (APPROVE, accepted). Reruns at the new head are in flight.

## Next

1. When the jobs pass: record them in Evidence, set State "ready for review" (F2 stays open: owner's call).
2. The mutant job 261110-021741-fbcbbf (c57ee057d: open lower slot 0, otherwise 9ea2fc771) must FAIL on the new key-order check.
3. If a job fails on something other than these tests, show it also fails at main and report.

## Blocked on

Owner, F2 (review query-dsl-fixes-correctness): range bucket default keys are built from the effective
bound in the field's type, so two requests distinct as written can share a key and be counted into one
bucket (integer `[{_from: 2.5}, {_from: 3}]` gives one `3..` = 6, brute force 3 each; positive
`[{_from: -5, _to: 4}, {_to: 4}]` gives `..4` = 4, brute force 2 each); visible keys also change
(`2.5..` shown as `3..`, `-5..4` on positive as `..4`). Merging by key was already at master for identical
requests. Options: (a) keep as is; (b) format the default key from the request's bounds as written
(`2.5..`, `-5..4`): keeps the requested text, stops distinct requests colliding, small and local to
`configure` (reviewer's recommendation); (c) key buckets by range index (root fix, outside this quest).
Not changed pending the owner.

## Evidence

Jobs: tests alone (38f22b685, no fix) fail: 261109-185346-2edf6c (exit 8, 8 FAIL) and under UBSan
261109-185357-8fe848 (exit 8). With the fix (ffc7e9517) both pass: 261109-193622-6d23b4 (RelWithDebInfo,
ASSERTS=ON) and 261109-193623-40183f (UBSan with float-cast-overflow, separate build and deps tree).
Examined and superseded: 261109-185347-50ec8a and 261109-185356-68cd98 (test setup bug, see Rulings 7),
261109-192344-c4ac9a and 261109-192345-1c7534 (bound-tag bug in my fix, Rulings 8).

| Criterion | Evidence |
|---|---|
| A1 | `test_dsl_not_is_none_of`: fails without the fix 261109-185346-2edf6c; passes with it 261109-193622-6d23b4 |
| A2 | `test_range_aggregation_open_bounds`: fails without the fix 261109-185346-2edf6c; passes with it 261109-193622-6d23b4; UBSan with it 261109-193623-40183f (without: 261109-185357-8fe848) |
| A3 | float and date cases in the same test (261109-193622-6d23b4). Note: negative float/date values with an open lower bound were dropped at master (min() is positive); the test counts them |
| A4 | served as the one-subquery form (Rulings 2); bare-object checks in `test_dsl_not_is_none_of`, 261109-193622-6d23b4 |
| A5 | build-test: 261109-193856-6e3365 (pending); CI after landing |

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
