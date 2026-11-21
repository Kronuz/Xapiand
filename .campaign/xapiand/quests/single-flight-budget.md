# single-flight-budget: A request joining a schema load leaves when its client does

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine
state is in `state.json`: `campaign status single-flight-budget`.

## Outcome

A request that joins another request's in-flight schema load in `SingleFlight`
(src/single_flight.h) waits without its own budget token, so its client leaving does not end its
wait (hunt row, from x7-cancel-rows; docs/mechanisms/cancellation.md Limits). After this quest a joiner
waits under its own token: when its client disconnects or its deadline passes, the joiner stops
waiting and answers its own cancellation, while the load and the other waiters continue. If
current main already does this, the result is that finding, with the job that shows it.

## Acceptance criteria

- A1 A test where request B joins request A's slow load and B's token fires first: B returns
  cancelled within a bound while A completes with the schema; it fails without the fix
  (`--fails-without` at main)
- A2 The suite passes at the landed code: the `suite` check
- A3 [artifact] docs/mechanisms/cancellation.md's Limits no longer lists the joiner, or says why it stays
- A4 [ci] CI is green

## Contracts

None.

## May land early

Nothing: one landing.
