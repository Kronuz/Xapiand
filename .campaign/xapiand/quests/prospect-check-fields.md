# prospect-check-fields: prospect's inverted reader forwards check_fields

Risk: low. The lead writes this quest; contributors read it and never edit it. Its
machine state is in `state.json`: `campaign status prospect-check-fields`.

## Outcome

`inverted_plugin::reader::check_fields(size, key)` forwards to its dictionary's check, so
strata's map-time check (field-check-at-map) covers prospect's segments.

Evidence: the eager-open explorer, 2026-11-21: `docs/perf/eager-open.md` on strata
`explore/eager-open` (strata:7107f9bc), prototype there and on prospect `explore/xapiand/eager-open`
(prospect:3f88c3c); identity job 261121-110632-e10bb1.

## Acceptance criteria

- A1 A dictionary with a field id outside the table, or the key field, makes `check_fields`
  throw; a good one passes: a test with values
- A2 prospect's suites pass (fielddict, fields, index, lifecycle, merge, multisegment,
  background_merge, reclaim): the `build-test` check
- A3 [ci] CI is green on every platform

## Contracts

Consumes strata:segment-check.

## May land early

Nothing: one landing.
