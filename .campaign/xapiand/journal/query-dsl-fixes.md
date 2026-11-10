# query-dsl-fixes: journal

State: waiting on jobs 261110-022726-fafa29 261110-022726-f81866 261110-022727-1dd71e 261110-022803-ef3749 261110-022824-c5a2f4
Quest: .campaign/xapiand/quests/query-dsl-fixes.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Rebased onto master (has A6 and D50). Code head Xapiand:c73e3c7f1: F4 print removed, F3 key-order test
(its `_sort` was misplaced beside `_range` in the first version, which failed 261110-021740-4105e9;
now inside `_range`), and A6 (owner's F2 = option b, D50): range bucket default keys are the request's
bounds as written (`bound_text`, `default_key` in bucket.h; `Bounds::key()` removed); the effective bounds
still drive matching and order. Sent to the reviewer's acceptance by confirm jobs.

## Next

1. When the jobs finish: confirm the two with-fix jobs pass, the A6 and F3 mutant jobs FAIL on their new
   checks (not elsewhere); set State "ready for review".
2. If a job fails on something other than these tests, show it also fails at main and report.

## Blocked on

Nothing. (F2 ruled by the owner: option b, D50, done as A6.)

## Evidence

Jobs at the code head Xapiand:c73e3c7f1 (journal commits after it change no code):
- test_engine_query with the fix, RelWithDebInfo, ASSERTS=ON: 261110-022726-fafa29.
- same under UBSan with float-cast-overflow: 261110-022726-f81866.
- build-test (`campaign verify`): 261110-022824-c5a2f4.
- A6 job, reviewed code with the new tests: 261110-022727-1dd71e runs Xapiand:163305050 (= reviewed
  Xapiand:f176d8896 with tests/test_engine_query.cc of the head; must FAIL on the two default-key checks).
- F3 mutant job: 261110-022803-ef3749 runs Xapiand:5ab7ab4b6 (head with the open lower slot 0; must FAIL
  on the key-order checks).
Without the fix: tests alone (38f22b685) 261109-185346-2edf6c (range test: A2, A3) and under UBSan
261109-185357-8fe848. Reviewer probe at master's query_dsl.cc with the final tests, part A:
261109-195413-671cc0 (A1, A4). Superseded: 261110-021740-4105e9 (my misplaced `_sort`), 261110-021741-fbcbbf
(same, mutant), 261109-193622-6d23b4 and 261109-193623-40183f (at ffc7e9517, before A6),
261109-185347-50ec8a, 261109-185356-68cd98 (test setup bug, Rulings 7), 261109-192344-c4ac9a,
261109-192345-1c7534 (bound-tag bug, Rulings 8).

| Criterion | Evidence |
|---|---|
| A1 | `test_dsl_not_is_none_of`: fails without the fix at master's query_dsl.cc (the `_not` checks and audit cases) 261109-195413-671cc0 (part A; 261109-185346-2edf6c failed on test setup first, not cited); passes with it 261110-022726-fafa29 |
| A2 | `test_range_aggregation_open_bounds`: fails without the fix 261109-185346-2edf6c; passes with it 261110-022726-fafa29; UBSan with it 261110-022726-f81866 (without: 261109-185357-8fe848); key-order check (Rulings 5) killed by mutant 261110-022803-ef3749 |
| A3 | float and date cases in the same test (261110-022726-fafa29). Note: negative float/date values with an open lower bound were dropped at master (min() is positive); the test counts them |
| A4 | served as the one-subquery form (Rulings 2); bare-object checks in `test_dsl_not_is_none_of`, fail at master 261109-195413-671cc0, pass 261110-022726-fafa29 |
| A5 | build-test at the head 261110-022824-c5a2f4; CI after landing |
| A6 | default keys `2.5..`/`3..` (integer) and `-5..4`/`..4` (positive), each with its own count, in `test_range_aggregation_open_bounds`: passes 261110-022726-fafa29; fails at the reviewed commit f176d8896 with the new test: A6 job 261110-022727-1dd71e |

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
