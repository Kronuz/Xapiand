> Status: CURRENT (2026-10-27) numeric-range workstream ledger; branch `fix/numeric-range`, local only.

# numeric-range (fix, from side findings)

Branch `fix/numeric-range` from Xapiand master `17e11d2cf`. Source: `docs/dev/side-findings.md` section 1. Mechanism:
`docs/mechanisms/numeric-range-terms.md`. Not pushed.

## State

In progress: decomposition written and property-tested standalone; engine build and E2E pending.

## Done

1. Step 2 answered: with a value slot the terms are a prefilter (`FILTER(value source, terms)`, `src/multivalue/range.cc`
   `getNumericQuery` and the date/time/timedelta twins), so a term set may over-cover and must never under-cover. A
   terms-only field (no slot) has no exact check.

## Next

1. Build, unit tests, revert check, mutants, ASan, E2E, A/B smoke, note, changelog.

## Blocked on

Nothing.
