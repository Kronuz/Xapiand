# query-dsl-fixes: _not over several subqueries and open-ended integer ranges

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status query-dsl-fixes`.

## Outcome

Two query defects found by the docs-site audit and confirmed by explorer job 261109-181823-84dfc6 at
master 0fffad7fc are fixed. `_not` over a list of subqueries matches the documents matching none of
them; today it builds `match_all AND_NOT (A AND_NOT B ...)` (`src/query_dsl.cc:287-294`, `:413-418`),
so `_not` over [female, brown, blue] returns all 7 of 7 documents instead of 0, and over [female,
brown] 5 instead of 2. A `_range` aggregation bound left open on an integer field covers every value
on that side; today the open bounds default to `long double` `min()` and `max()` (`bucket.h:1094`,
`:1109`) and are narrowed to int64 at `:1128`, so an open lower bound becomes 0 and the open upper
bound is undefined behavior: `[{"_to":10},{"_from":0}]` over 3, 10, 2^62, -2^62 gives one bucket of
1 instead of "..10"=2 and "0.."=3.

## Acceptance criteria

- A1 `_not` over N subqueries (N = 1, 2, 3) returns exactly the documents matching none of them, checked by ids against a brute-force filter over the same documents: a test in `tests/test_engine_query.cc` that fails at master's code (job at master, or the fix reverted), run by the `build-test` check
- A2 Open-ended `_range` buckets on integer fields (only `_from`, only `_to`, both open sides in one request, negative bounds, values at the int64 extremes) count exactly what a brute-force count gives, with no undefined behavior: a test that fails at master's code, plus the same test under UBSan as a job
- A3 Float and date range aggregations with open bounds keep their counts (no regression): in A2's test
- A4 `_not` with a bare object (`{"_not": {...}}`) is either served as the one-subquery form or keeps its 400 with a message naming the expected list; the journal says which and why (lead's preference: serve it, as `_and`/`_or` accept): in A1's test
- A5 The `build-test` check passes, and CI is green
- A6 Range buckets' default keys come from the request's bounds as written (D50): integer `[{_from:2.5},{_from:3}]` gives two buckets `2.5..` and `3..`, positive `[{_from:-5,_to:4},{_to:4}]` gives `-5..4` and `..4`, each with its own count; a test that fails at the reviewed commit f176d8896, run by `build-test`

## Contracts

The query DSL and aggregation request shapes are unchanged; only their results become correct.

## Decisions

- Lead, 11-09: a fix, not a narrowing. The docs pages for `_not` and range aggregations get E2E tests in docs-site-refresh (D47).

## May land early

Nothing; one landing.
