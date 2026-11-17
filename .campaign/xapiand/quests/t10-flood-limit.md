# t10-flood-limit: A long Lua call yields at each slice, so it turns heavy

Risk: normal (loop scheduling). The lead writes this quest; contributors read it and never edit it.

## Outcome

FreeBSD CI's T10 row (a light GET beside 16 clients flooding the Lua per-call cap) fails at
110-155 ms. The exploration (`explore/t10-flood`, Xapiand:68e12aa7c, docs/dev/t10-flood-limit.md)
found a real scheduling gap: a request turns heavy at its 8th spent slice (loop-scheduling D1,
`REQUEST_LIGHT_SLICES`), but a non-yielding ~22 ms call is seen past the 200 us slice once and counts
as one slice, so flood requests stay light and the GET queues behind every flooder on its reactor
(p99 22/24/47/87/134 ms at 1/2/4/8/16 clients on one reactor). Charging an over-slice turn afterwards cannot help (a flood
request runs its one call in its first light turn and ends), so the call itself stops being one
stretch: a Lua count hook yields the script's coroutine at each slice boundary, so a 22 ms call spends
~110 slices, turns heavy after 8 under the existing rule, and "one heavy turn per pass" bounds the
light wait.

## Acceptance criteria

- A1 A Lua call longer than a slice yields at each slice boundary and its request turns heavy at the
  8th slice, with the script's result and the per-call cap unchanged: unit tests with values (a
  yield count, the lane at each slice, a result identical to master's), and a mutant without the
  hook's yield failing them (`--fails-without`).
- A2 The flood sweep (1/2/4/8/16 clients, one reactor; `.scratch/t10/sweep.py`) shows the light GET's
  p99 flat in the client count, within a bound derived from the scheduler's rule (k flood calls, k
  stated), at the fix and growing at master: a strict job at each commit.
- A3 T10's flood limit is that derived bound (no constant tuned to a host), and FreeBSD CI's rows pass
  at the landing commit: [manual] the run's T10 log, by the lead.
- A4 [artifact] `docs/design/loop-scheduling.md` states the over-slice rule, and
  `docs/dev/t10-flood-limit.md` the derivation.
- A5 The suite passes (the `suite` check).

## Decisions

- Lead: fix the mechanism, not the limit (the derived multiple failed at 155 ms against 128.8); the
  task-side charge was dropped as unable to reclassify a one-call request.

## May land early

None.
