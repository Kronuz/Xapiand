# hunt-htm-constexpr-start: htm start tables constant-initialized

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status hunt-htm-constexpr-start`.

## Outcome

Hunt row (hunt-header-tables, Xapiand:073d466 docs/dev/header-tables.md, "Library rows"):
htm.h's `start_vertices` builds 6 `Cartesian`s through a non-constexpr constructor in each of the
35 including TUs, about 6.7 KB of initializers inferred, not proven. Measure first: confirm the
initializer exists and its size. If it does, a constexpr `Cartesian` constructor (and constexpr
start tables) makes them constant-initialized; if the measurement shows no initializer, stop and
record that in the Result.

## Acceptance criteria

- A1 The suite passes: the `build-test` check, a job tagged `--for A1`
- A2 [artifact] The journal's Result gives, at named commits, the static initializer of one
  including object file before and after (or the measurement showing there was none)
- A3 Values are unchanged: a test compares start_vertices and start_trixels with the old values
  (a job), unless A2 shows nothing to change
- A4 [ci] CI is green on every platform

## Contracts

None: no API or behavior change.

## Decisions

- Lead, 2026-11-23: low risk, an exploration batch target; the lead reads the diff, no review.

## May land early

Nothing: one change.

## Tasks

None: one contributor (exploration, economical, medium).
