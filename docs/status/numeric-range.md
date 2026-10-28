> Status: CURRENT (2026-10-27) numeric-range workstream ledger; branch `fix/numeric-range`, local only, CR1 fixed, ready for re-check.

# numeric-range (fix, from side findings)

Branch `fix/numeric-range` from Xapiand master `17e11d2cf`. Source: `docs/dev/side-findings.md` section 1. Mechanism:
`docs/mechanisms/numeric-range-terms.md`. Not pushed, no CI run.

## State

Built and proven locally; CR1 fixed (Xapiand:9ee98a9cb), ready for re-check. Index-time terms are unchanged (no reindex).

## Done

1. Terms are a prefilter, not the whole filter: with a value slot `getNumericQuery` and the date/time/timedelta twins build
   `FILTER(value-range source, terms)` (`src/multivalue/range.cc`, the `field_spc.slot != engine::BAD_VALUENO` branches), so
   terms may over-cover and must never under-cover. A terms-only field has no exact check (budget 4096, ends widened to the
   finest bucket).
2. `range_terms::decompose` (`src/multivalue/range_terms.h`) replaces `_numeric`'s walk: whole buckets from the coarsest
   accuracy down, ends widened to the finest accuracy that fits 128 terms, no term filter past that. Xapiand:76097e087 and
   Xapiand:3dc64ef63.
3. Open ends: type limits instead of `min/max<T>(accuracy)` (2x the coarsest accuracy) and `DBL_MIN` (positive); a date range
   with an open end has no terms (was cut at 1970 and 2038; `date >= 2040` returned nothing).
4. Date/time/timedelta checked: time and timedelta share `_numeric`; the closed-range datetime walk (`GenerateTerms::datetime`)
   is correct (engine test over three accuracy lists, 1200 ranges, 0 missing, also on master). Geo is out of scope (HTM trixels).
5. Proof.
   - Property test `tests/test_range_terms.cc` (321553 checks, 0 wrong): every range on a small domain for nested and non-nested
     accuracies and budgets 1 to 128; 54000 random and boundary ranges over the 64-bit domain, signed and unsigned.
   - Engine test `tests/test_generate_terms.cc` (real query over indexed documents): fails on master (1 of 12), passes now.
   - Mutants: three header mutants caught by the property test; wiring mutant (`acc_prefix[0]`) caught by the engine test.
   - ctest 20/20 Release; ASan+UBSan unit tests and `numeric_range_check.py` pass with `halt_on_error=1`.
   - Live server `harness/numeric_range_check.py` (797 ranges: integer, positive, floating, datetime; closed and half-open):
     master 399 wrong (n 157/206, p 140/197, f 61/197, d 41/197), now 0.
   - Docs E2E 576/576 (newman 6.2.3), `e2e_assert.py` GREEN. `harness/id_terms_check.py` OK; with the 2^28 ids
     (`ID_TERMS_EXTRA=268435455,268435456,268435457`) master FAIL (3 missing), now OK.
   - Query smoke A/B, 200k docs, 380 range queries, screen only (not a measurement): master gave 110 wrong counts, now 0.
     Narrow and mid ranges are unchanged; wide (5e8) 2.5 s to 3.2 s and open-ended 0.95 s to 3.1 s for 80/60 queries, because
     master returned fewer documents (it lost the buckets); open ends now run on the value source alone.
6. No fuzz target: `decompose` takes numbers, not bytes; the property test is its hardening.

## Fix round (code review `docs/reviews/numeric-range-code.md`)

1. CR1 (blocker): a terms-only field (no value slot) throws `TermsOnlyRangeError` (400, field named) for an open-ended range, a
   range wider than 4096 coarsest buckets, and a datetime range with an empty calendar cover, instead of an empty query (0 hits,
   200). `require_terms_only_cover` in `src/multivalue/range.cc`; `QueryDSL::get_in_query` adds the field name. Engine-level
   tests in `tests/test_generate_terms.cc` (CR4): terms-only and open ends for integer, positive, floating, time, timedelta,
   datetime and date. Stated in `docs/mechanisms/numeric-range-terms.md`; changelog line.
   Proof: the new engine test fails 23 of 50 checks with the check disabled and passes with it; a mutant (the too-wide branch
   off) fails 7; ctest 20/20; `numeric_range_check.py` 0 wrong of 797; reviewer's `live_probe.py`: value-slot rows identical to
   the review's branch run, terms-only rows 0 missing and 0 empty-while-nonempty (were 124 to 229 per field), open-ended and
   wide ranges now 400 with the field named (the timedelta rows over 99 hours still 400 from the parser); docs E2E 576/576
   (newman 6.2.3); ASan+UBSan (`halt_on_error=1`) `test_generate_terms` 50 checks and `test_range_terms` 321553, clean.
2. CR2: left as is (see Open).
3. CR3 and CR5: recorded under Open, out of scope.

## Next

1. Re-check of CR1. Lead: CI.

## Open

Not this branch's scope; each needs its own owner.

1. CR2 (architect follow-up): open-ended ranges run on the value source alone; a prefilter needs the shard's value bounds at
   query-open time (`get_value_lower_bound(slot)` / `get_value_upper_bound(slot)`, per shard in a cluster). Reviewer's screen
   (200k docs, `n` uniform in [0, 1e9), medians of 7): `n >= 999000000` (187 hits) 9.1 ms open vs 2.0 ms bounded; `n <= 1000000`
   7.9 vs 1.9 ms; `n >= 990000000` 9.7 vs 3.9 ms; `n >= 900000000` 13.6 vs 8.4 ms; `n >= 500000000` 29.9 vs 31.2 ms. About 35 ns
   per document, on selective open-ended queries only (master returned 0 documents for them).
2. CR3: the value-slot compare is inexact for some integers, identical on master and branch: a positive field holding
   2147483647..2147483651, `_from: 2147483649` returns [2147483649, 8796093022209] (misses 2147483650 and 2147483651), `_to:
   2147483649` also returns 2147483650 and 2147483651, `[2147483649, 2147483650]` returns nothing (probe `one2.py`). Seen from
   2^31 up to at least 2^43 and for negatives near -2^43 to -2^62.
3. CR3: two custom-accuracy integer fields in one schema (`c` {7,100,3000}, `c2` {3,5,11,1000}, same values): `c` with
   `_to: 70` misses the ten most negative documents (`live_probe.py`, `ONLY=c,c2`); not with `c` alone, or with `n` or `tc`.
   Open-ended means no terms, so the source or slot handling.
4. CR5: `static_cast<int64_t>(double)` at |f| >= 2^63 and NaN is undefined on the index side (x86 gives INT64_MIN, arm64
   saturates); `truncate_saturating` assumes the saturating result. Use a saturating cast on the index side.
5. CR5: `RESTORE` drops the first document of an ndjson body (a 200k-document restore holds 199999; `miss.py`).

## Findings outside scope

1. `modulus` in the Kronuz `math` library negates `INT64_MIN` (signed overflow): indexing that value trips UBSan. Fix there;
   the tests index `INT64_MIN + 1`.
2. Datetime around 1900-03-01 disagrees with Python's calendar by one day (1900 treated as a leap year); not a term issue.

## Blocked on

Nothing.
