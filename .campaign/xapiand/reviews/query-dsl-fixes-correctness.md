# query-dsl-fixes review: correctness

Commit: ace371929756 (rebased; quest files identical to the re-reviewed Xapiand:c73e3c7f1a048adfda487f7879cf2ef672480724)
Verdict: APPROVE
Date: 2026-11-10

Two rounds, both kept below. The first round reviewed Xapiand:f176d8896f4089a5b6dbc7680f390a83b56386f0 (code at Xapiand:ffc7e9517) on 2026-11-09. The re-review covers F2 and F3 at c73e3c7f1. `git diff c73e3c7f1 ace371929756 -- src/query_dsl.* src/aggregations tests/test_engine_query.cc` is empty (checked 2026-11-10).

APPROVE: lands as is, at this commit's code. CHANGES: lands after each blocking finding is fixed
and confirmed by a job at the fixed commit. REJECT: the approach is wrong; the quest is re-planned.
An APPROVE lists no blocking finding; a CHANGES lists at least one.

## Evidence I relied on

- 261109-193622-6d23b4 (ffc7e9517, RelWithDebInfo, ASSERTS=ON): `test_engine_query` passes with both new tests.
- 261109-193623-40183f (ffc7e9517, separate build and deps tree, `-fsanitize=undefined,float-cast-overflow -fno-sanitize-recover=all`): passes. Its counterpart without the fix, 261109-185357-8fe848, fires `1.18973e+4932 is outside the range of representable values of type 'long long'`, so the sanitizer is live on this path. This settles A2's "no undefined behavior".
- 261109-185346-2edf6c (tests only, 38f22b685): the range test fails at master's bucket.h (A2, A3). It does **not** show A1: `test_dsl_not_is_none_of` failed there on the test's own setup ("g=female is not a term query", then "The index rejected the operation") before any `_not` check ran. See F1.
- 261109-192344-c4ac9a (a8ca32c, the fix before ffc7e9517): fails only on the positive-field `within` bucket. That is a real mutant (the tag-reading bug) killed by the test.
- 261109-193856-6e3365 (build-test at f176d8896f40): passes.

## What I checked myself

Probe job 261109-195413-671cc0 at f176d8896f40 with `--ref BASE=38f22b685701`. Scripts are
`.scratch/review-query-dsl-fixes-correctness/probe.sh` and `probe.inc`; rerun with
`campaign job submit --quest query-dsl-fixes --commit <commit> --ref BASE=38f22b685701 --slot qdf-review -- sh .scratch/review-query-dsl-fixes-correctness/probe.sh`.

1. A: the final test file, with `src/query_dsl.cc` taken from BASE. All 11 subsets whose expected set differs from the old result fail (line 1013), and so do the three audit cases (1028-1030). The bare object then throws `QueryDslError: Invalid operator: _not`. A1 and A4 fail at master's code.
2. B: the fixed code plus probe tests, all passing. Checked by ids against hand-computed sets:
   - `_not: []` gives all 7 documents (master behaved the same).
   - `_not` inside `_or`, `_not` over an `_and`, double `_not`, and `_not` over `[_or, term]` all give the right ids.
   - A multi-field map element (`_not: [{g: male, e: blue}]`) and its bare form both mean "none of its fields" (1 3 7), as Ruling 1 says.
   - The field-scoped forms `{g: {_not: "female"}}` and `{e: {_not: [brown, blue]}}` give the right ids.
   - `_and_not` is unchanged (A AND_NOT B), and `_and` with a bare `_not` gives the right ids.
3. B against C (C is BASE's bucket.h plus the probe): default keys, counts and output order of closed ranges are identical on integer (small and ±2^62), float, date and positive fields. The only differences are the cases in F2.
4. By reading `reach_of` (bucket.h): for int64 and uint64, `lowest` and `beyond` are exact doubles (-2^63 or 0, and 2^63 or 2^64). The cast happens only for `d` in [lowest, beyond), so it is exact and free of UB. ±inf go below or above, and NaN is a 400. `ceil` is the exact integer edge for both `>=` and `<`. -0.5 on a positive field ceils to -0.0, which casts to 0 without UB. A negative-tagged non-negative value reads as its value. Keyed and unkeyed output, `idx`, and the default `by_index` order sit in the base class and are unchanged. `slot` changes only for an open lower bound (-inf, intended) and for fractional integer bounds (the rounded edge).
5. The query-string `NOT` (`create_exp_op_dsl`) emits `{_not: [x]}`, a list of one, so its meaning is unchanged. The new bare-object `case` is the only addition to the non-array switch; no other operator's parsing moves.
6. Stray output: `git diff master...f176d8896f40 -- src tests` adds exactly one unconditional print, `positive bucket %s = %lld` (tests/test_engine_query.cc, in the positive-field block of `test_range_aggregation_open_bounds`). Every other added `fprintf` prints only on a failure.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | no | gap | The journal cites 261109-185346-2edf6c as A1 "fails without the fix", but that run failed on test setup before any `_not` assertion. A1 at master's code is now established by probe A. | 2edf6c log lines 7119-7121; probe 261109-195413-671cc0 run-A | Cite 261109-195413-671cc0 (part A) for A1 and A4 in the Evidence table. |
| F2 | no | defect | The default key is now built from the effective bound in the field's type, not the requested one. Buckets merge by key string (`aggregate()` finds by key), so two ranges that are distinct as requested but share an effective key are counted into one bucket, doubled. On integer, `[{_from: 2.5}, {_from: 3}]` gives one `3..` = 6 (the brute force gives 3 each). On positive, `[{_from: -5, _to: 4}, {_to: 4}]` gives `..4` = 4 (brute force 2 each). The visible keys also change: `{_from: -5, _to: 4}` on positive is shown as `..4`, `{_from: 2.5}` on integer as `3..`, and an out-of-type bound as open. Merging by key is pre-existing (identical requested ranges merged at master too); the rounding makes it reachable from distinct requests. Owner's call; options: (a) keep as is; (b) format the default key from the request's bounds as written (`2.5..`, `-5..4`), which keeps the requested text visible and stops distinct requests colliding; (c) the root fix, keying buckets by range index (outside this quest). | probe 261109-195413-671cc0 keys-B vs keys-C (`int-frac`, `pos-negfrom`); `Bounds::key()` and `configure` in src/aggregations/bucket.h | Recommend (b), small and local to `configure`. |
| F3 | no | gap | Ruling 5 (open lower bound has slot -inf, so it sorts first under `_sort: _key`) is not tested; every test uses the default `by_index` order, so a mutant that sets slot to 0 survives. | tests/test_engine_query.cc `range_counts` sets no `_sort` | One request with `_sort` by key ascending and an open-lower range alongside negative ranges. |
| F4 | no | nit | Unconditional debug print `positive bucket %s = %lld` left in the test (from Xapiand:a8ca32c). It is the only stray output. | tests/test_engine_query.cc, positive-field block of `test_range_aggregation_open_bounds` | Delete the loop. |
| F5 | no | nit | A NaN bound is a 400 on integer and positive fields ("bounds must be numbers") but silently matches nothing on float and date fields. Only msgpack bodies can carry NaN. | `reach_of` in src/aggregations/bucket.h | Optional: apply the same check in the floating branch. |

## Not run

- Range subqueries (`_range`/`_in`) under `_not`: not run. They go through the same OR path that the term, `_and` and `_or` subqueries exercise.
- JSON-parsed request bodies through the HTTP layer: not run. The tests build bounds from C++ integers, which carry the negative tag. A JSON-parsed non-negative bound on an integer field takes the POSITIVE_INTEGER branch, which a test reaches only through `past_max`; it is a one-line cast.
- No ASan or TSan run: no allocation or threading change.
- No Linux or other platforms: CI covers those.
- No distributed `merge_results` path: unchanged code.

## Re-review

Commit: c73e3c7f1a048adfda487f7879cf2ef672480724
Verdict: APPROVE
Date: 2026-11-10

Scope is F2 (option b, D50 and A6) and F3: `git diff f176d8896f40..c73e3c7f1 -- src/aggregations/bucket.h tests/test_engine_query.cc` (bucket.cc is unchanged).

Evidence I accepted:
- 261110-022726-fafa29 (RelWithDebInfo, ASSERTS=ON) and 261110-022726-f81866 (UBSan with float-cast-overflow) pass at c73e3c7f1.
- 261110-022727-1dd71e runs the new test file over the bucket.h I approved earlier: `git diff f176d8896f40 163305050350 -- src` is empty and the test file is identical to c73e3c7f1's. It fails only at :1270 and :1280, the two collision cases, so the F2 test fails without the fix.
- 261110-022803-ef3749 is exactly the F3 mutant (`slot()` returns 0 for an open lower bound). It fails only at :1309 and :1310.
- 261110-021740-4105e9 failed at 9ea2fc771c6b, a commit before c73e3c7f1. The test it ran is not the one at the head, and I did not review that test.

What I checked by reading:
- **Bounds as written.** `bound_text` prints an integer tag as its exact integer and a float as fmt's shortest round-trip text (`2.5`, `-1e+09`, `1e+30`). A missing bound is an empty side, so an open end gives `X..` or `..Y`, and no bounds gives `..`. Dates take only numeric bounds, as before.
- **Effective bounds still drive behaviour.** `Bounds` (`contains`, `slot`) still uses the effective bounds, so matching and ordering are unchanged. The test confirms it: `2.5..` counts 3 and 5.
- **Named `_key`.** The `key.empty() ? default_key(range) : key` path is the same as before.
- **Collisions.** Two requests with different effective ranges can no longer share a default key. Equal key text means equal bound values (round-trip float text and exact integers), and `..` cannot occur inside a bound's text.
- **F3 test.** `_sort` sits inside `_range`, where `_conf_sort` reads it. Every bucket in it is non-empty, so the asc and desc orders test the slot and nothing else.

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| R1 | no | nit | Two writings of the same value still share a default key and merge counts: `{_from: 3}` and `{_from: 3.0}` both give `3..`. They are the same range, as were the duplicate requests that already merged before this quest. A msgpack float32 bound prints its widened double (`0.10000000149011612`). | `bound_text` in src/aggregations/bucket.h | None needed; keying buckets by range index would remove the merge entirely (outside this quest). |

F2 and F3 are resolved. F1 (evidence citation) is the lead's to confirm in the journal.
