# prospect-completion-opens: prospect's test IOs implement completion's four read-only opens

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status prospect-completion-opens`.

## Outcome

completion:c315c03 made `kronuz::journal::IO`'s four read-only opens pure virtual. prospect's
test IO wrappers (`gate_io`, `wrapper_io`, `flaky_io`) pass them through, so prospect's suite
builds against the strata barrier-warning-strata lands (which pins completion:e49f288).

## Acceptance criteria

- A1 prospect's suite passes against barrier-warning-strata's strata: barrier-warning-strata's A4 integration job, at this quest's commit
- A2 [artifact] The three wrappers forward all four opens: `test/index_test.cc`

## Contracts

Consumes completion's journal IO interface.

## Decisions

- Lead, 11-12: split from barrier-warning-strata's A4 (the wrappers use `override`, so this lands
  after strata's pin moves; prospect's repin follows).

## May land early

Nothing: it does not build at prospect's current pins.
