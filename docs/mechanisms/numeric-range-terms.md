> Status: CURRENT (2026-10-27) how numeric range queries pick their accuracy terms; workstream numeric-range, branch `fix/numeric-range`.

# Numeric range terms

A numeric field (integer, positive, floating, time, timedelta) is indexed under accuracy terms: for each accuracy `a` of the
field (default `100, 1e3, 1e4, 1e5, 1e6, 1e8`) the term of the bucket `[k*a, (k+1)*a)` that holds the value, with `k` the floor
of `value / a` (negatives floor, the 64-bit result wraps like the index). A range query `[lo, hi]` becomes
`FILTER(value-range source, OR(bucket terms))` in `src/multivalue/range.cc`.

## Terms are a prefilter

With a value slot (every field indexed with values, the default) the exact value source decides, and the terms only
narrow what it looks at. So the term set may match values outside the range and must match every value inside it: a
bucket missed is a document silently lost. A terms-only field (`_index: "field_terms"`, no slot) has no exact check: its
closed-range answer is the union of the buckets, wider than the range by less than one finest bucket per end for the numeric kinds (datetime terms can be wider), and
it gets a larger budget (`TERMS_ONLY_MAX_TERMS`, 4096).

## Terms-only fields

A range that the terms cannot cover is an error on a terms-only field, never an empty or an over-covering answer: an
open-ended range (`_from` or `_to` alone), a range wider than the budget (4096 coarsest buckets), and a datetime range whose
calendar terms come out empty. The query throws `TermsOnlyRangeError` (a `QueryParserError`, HTTP 400); `QueryDSL` adds the
field name: "Cannot answer a range on field ...: the range is open-ended ... / too wide ...; index its values (not only its
terms) to make ranges exact". An inverted range (`_from` above `_to`) is empty, not an error. A field with no accuracies at all
matches every document, including those without the field: a superset, as on a value-slot field. With a value slot none of this applies: an
empty cover means "no prefilter" and the value source decides.

## Decomposition

`range_terms::decompose` (`src/multivalue/range_terms.h`) is the prefix-coded range split used for numeric tries (Lucene's
`NumericUtils.splitRange` is the reference idea), over arbitrary accuracies:

1. Work on the half-open interval `[lo, hi+1)` in 128-bit integers, so no bound overflows.
2. From the coarsest accuracy down, emit every bucket that lies wholly inside a remaining piece; what is left of a piece at
   each end goes to the next finer accuracy.
3. What remains after the finest accuracy in use is narrower than one of its buckets: widen it to the bucket(s) it touches.

The terms are the buckets, ORed flat. Accuracies need not nest.

## The budget

At most `MAX_TERMS` (128) terms. `decompose` tries the finest accuracy first and, when the plan needs more terms, drops it:
the ends are then widened to the next coarser accuracy (fewer, wider end buckets; the exact check removes the extras). If
even the coarsest accuracy alone needs more than the budget (a range wider than 128 coarsest buckets) there is no term
filter: `GenerateTerms::numeric` returns the empty query and the value source decides alone. Coverage is never dropped to
fit the budget.

## Open ends

A missing bound is the type's limit (not a multiple of the coarsest accuracy), so `{_to: 5}` is `[min, 5]`, which has no
term cover and runs on the value source (an error on a terms-only field). A date range with an open end likewise has no terms.

## Dates

Date and datetime ranges use calendar buckets (`GenerateTerms::datetime`): one "needed" accuracy from the largest differing
calendar unit, ANDed with the next coarser accuracy, a handful of terms either way. Their open ends were bounded by
`TIMESTAMP_MIN`/`TIMESTAMP_MAX` (1970 and 2038), which dropped dates outside them; an open end now has no term filter (an error on a terms-only field).

## Tests

`tests/test_range_terms.cc` checks the decomposition against brute force (every range on a small domain for nested and
non-nested accuracies and tight budgets; random and boundary ranges over the whole 64-bit domain, signed and unsigned).
`tests/test_generate_terms.cc` runs the real query through the engine over indexed documents, numeric and datetime, and checks
the terms-only errors for every kind.
`harness/numeric_range_check.py` runs closed and half-open `_range` queries against a live server.
