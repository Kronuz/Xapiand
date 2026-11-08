# numeric-range: Prevent numeric range term undercoverage

Risk: normal. History: this landed before the campaign, under the earlier campaign skill, at Xapiand:5f356d78f (2026-10-27); its ledger `docs/status/numeric-range.md` is the record. Its machine state is in `state.json`: `campaign status numeric-range`.

## Outcome

Numeric range terms provide a sound prefilter for value-slot fields, widening end buckets or dropping the filter when needed. Terms-only fields reject ranges they cannot cover instead of silently returning empty results.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] Terms are a prefilter for value-slot fields and never under-cover; terms-only fields use a 4096-term budget: `docs/status/numeric-range.md`
- A2 [artifact] Replace `_numeric` bucket walk with `range_terms::decompose`, widening ends to fit 128 terms or dropping the filter: `docs/status/numeric-range.md`
- A3 [artifact] Use type limits for open ends and avoid incorrect date bounds; check date, time, and timedelta behavior: `docs/status/numeric-range.md`
- A4 [artifact] Property test 321553 checks and engine test over indexed documents; mutants catch decomposition and wiring errors: `docs/status/numeric-range.md`
- A5 [artifact] Live range harness: master 399 wrong, branch 0 wrong of 797; E2E 576/576: `docs/status/numeric-range.md`
- A6 [artifact] Review CR1: throw `TermsOnlyRangeError` for uncovered terms-only ranges and test integer, positive, floating, time, timedelta, datetime, and date: `docs/status/numeric-range.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Terms-only ranges with no sound cover throw a named 400 error, rather than returning an empty or over-covering result.
- CR2 open-ended value-slot ranges run on the value source alone; shard bounds are a separate follow-up.
- Risk normal: Corrects range-query results without changing index-time terms (the lead's mapping).

## May land early

None.
