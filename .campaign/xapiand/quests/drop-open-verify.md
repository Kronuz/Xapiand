# drop-open-verify: Xapiand drops its open-time field check; strata checks at map

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its
machine state is in `state.json`: `campaign status drop-open-verify`.

## Outcome

Xapiand deletes `verify_fields` from `borrow`, `acquire` and `open_index`
(`src/engine/strata/shard.cc`), keeping it in `check()`, once strata checks field ids at every
map (field-check-at-map) and the strata and prospect pins include it. The check cost 30 to 64 us
per segment per open, 22 to 31% of an eager open at 4 and 10 segments.

Evidence: the eager-open explorer, 2026-11-21: `docs/perf/eager-open.md` on strata
`explore/eager-open` (strata:7107f9bc), prototype there and on prospect `explore/xapiand/eager-open`
(prospect:3f88c3c); identity job 261121-110632-e10bb1.

## Acceptance criteria

- A1 A corrupt index (a field outside the table) still answers an error on its first request,
  with the same status as today: a test with values
- A2 Opening an index costs less: Xapiand's open of a 4-segment index, before and after, `strict`
  jobs with an A/A, honest magnitude
- A3 The suite passes: the suite check
- A4 [ci] CI is green on every platform

## Contracts

Consumes strata:segment-check.

## May land early

Nothing: one landing.
