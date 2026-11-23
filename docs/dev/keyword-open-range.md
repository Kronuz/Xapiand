# Keyword and text ranges with only a lower bound

Hunt row M2: `{"_from": "b"}` on a keyword or text field matched nothing.

## Cause

`getStringQuery` (src/multivalue/range.cc) serialized an absent bound as `""` and then compared
`start_s > end_s` unconditionally. A lower bound alone gives `"b" > ""`, so it returned the empty query
before reaching the `MultipleValueGE` branch. An upper bound alone (`"" > "x"` is false) and both bounds
were not affected. The same function serves `uuid`, `boolean`, `keyword` and `text`.

The numeric, date, time and timedelta paths do not share it: numeric uses `-inf`/`+inf` (and the type's
min/max), date compares only when both ends are given, time and timedelta use `lowest()`/`max()`. Only the
string path was wrong; src/multivalue/generate_terms.cc is not involved (it builds no string ranges), so
cjk-words' files stay untouched.

## Fix

One line in `getStringQuery`: `if (start && end && start_s > end_s)`.

## Test

tests/test_coverage_multivalue_range.cc, `test_string_range_query`: the lower-bound-only case, which asserted
the empty query (documenting M2), now expects the `MultipleValueGE` source for keyword and text, and the
boolean case gains lower-only and upper-only expectations. Upper-only and both-bounds string cases were
already covered. Date, time and numeric open ranges are covered in the same file.

## Evidence

Jobs (`harness/coverage_one.sh multivalue_range src/multivalue/range.cc`):

1. Test without the fix, commit 0703f2b27: job 261121-042222-ac72f6 (expected to fail on the lower-bound CHECK).
2. Test with the fix, commit fd33f492a: job 261121-042220-4f9ad8 (expected to pass).

Files: src/multivalue/range.cc, tests/test_coverage_multivalue_range.cc. Plan it as a quest after cjk-words
only if it needs to wait; range.cc is not in cjk-words' file set, so it can land directly.
