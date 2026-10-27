> Status: CURRENT (2026-10-27) numeric-range workstream ledger; branch `fix/numeric-range`, local only, ready for review.

# numeric-range (fix, from side findings)

Branch `fix/numeric-range` from Xapiand master `17e11d2cf`. Source: `docs/dev/side-findings.md` section 1. Mechanism:
`docs/mechanisms/numeric-range-terms.md`. Not pushed, no CI run.

## State

Built and proven locally; ready for code review. Index-time terms are unchanged (no reindex).

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

## Next

1. Code review. Lead: CI.

## Findings outside scope

1. `modulus` in the Kronuz `math` library negates `INT64_MIN` (signed overflow): indexing that value trips UBSan. Fix there;
   the tests index `INT64_MIN + 1`.
2. Datetime around 1900-03-01 disagrees with Python's calendar by one day (1900 treated as a leap year); not a term issue.

## Blocked on

Nothing.
