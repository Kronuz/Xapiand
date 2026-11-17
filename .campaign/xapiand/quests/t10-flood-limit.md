# t10-flood-limit: A script call runs in the heavy lane

Risk: normal (loop scheduling). The lead writes this quest; contributors read it and never edit it.

## Outcome

FreeBSD CI's T10 row (a light GET beside 16 clients flooding the Lua per-call cap) fails at
110-155 ms. The exploration (`explore/t10-flood`, Xapiand:68e12aa7c, docs/dev/t10-flood-limit.md)
found a real scheduling gap: a request turns heavy at its 8th spent slice (loop-scheduling D1,
`REQUEST_LIGHT_SLICES`), but a non-yielding ~22 ms call is seen past the 200 us slice once and counts
as one slice, so flood requests stay light and the GET queues behind every flooder on its reactor
(p99 22/24/47/87/134 ms at 1/2/4/8/16 clients on one reactor). Neither a later charge nor the count hook helps: the flood's one
`s:find` is a single C call with no VM instruction inside, so it runs as one ~22 ms stretch. The
script call therefore runs in the heavy lane and returns to light after (heavy by what the request
runs, not by route), so a light request waits about k = 2 stretches, flat in the client count.

## Acceptance criteria

- A1 A request's script call runs in the heavy lane and the request is light again after it, with
  results unchanged: tests with values, and a mutant keeping the call light failing them
  (`--fails-without`). A tiny scripted write's p50/p99 at idle and beside a light-only load are no
  worse than master's: strict jobs at both commits.
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
