# prospect-completion-opens: prospect's tests build and pass against strata's lazy segments and completion's read-only opens

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status prospect-completion-opens`.

## Outcome

completion:c315c03 made `kronuz::journal::IO`'s four read-only opens pure virtual. prospect's
test IO wrappers (`gate_io`, `wrapper_io`, `flaky_io`) pass them through, so prospect's suite
builds against the strata barrier-warning-strata lands (which pins completion:e49f288).

strata:6d4e1164 made lazy segments the default (cold-tier step 1; ruled 2026-11-10 in strata
`docs/status/cold-tier.md` K7: lazy stays, prospect's tests change at its pin bump). Six damaged-segment
cases in format13_test (:603, :676, :783) and index_test/index_mapped (:1399, :1795) expect the open
to fail; under lazy segments the open succeeds and the first map throws `corrupt_segment`. Those cases
open, read through `segments()[0].seg()` or a `get`, expect `corrupt_segment`, and check `needs_heal()`
and `heal()` report the segment; they never set `eager_segments`. A missing or resized segment file
still fails the open (unchanged).

## Acceptance criteria

- A1 prospect's suite passes against barrier-warning-strata's strata: barrier-warning-strata's A4 integration job, at this quest's commit
- A2 [artifact] The three wrappers forward all four opens: `test/index_test.cc`
- A3 The six damaged-segment cases pass under the lazy default and fail if the read no longer reports `corrupt_segment` (a `--fails-without` job), with `eager_segments` unset: jobs

## Contracts

Consumes completion's journal IO interface.

## Decisions

- Lead, 11-12: split from barrier-warning-strata's A4 (the wrappers use `override`, so this lands
  after strata's pin moves; prospect's repin follows).

## May land early

Nothing: it does not build at prospect's current pins.
