# hunt-radix-router: radix-router: bound slow matching inputs found by fuzzing

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status hunt-radix-router`.

## Outcome

Route matching in Kronuz/radix-router costs time bounded by the path's length times a small
constant, for any input, so a client path cannot make a request's routing slow. The fuzz
continuation (Codex C4, 11-14) timed out at 333 CPU s of 3,600 on slow matching inputs; their
class is named, bounded, and kept as regression tests.

## Acceptance criteria

- A1 The slow input class is found on current main and named: a committed reproducer whose
  matching time on main is stated, from a strict job (or the note says nothing slow reproduces,
  and the quest ends with that note).
- A2 Matching is bounded: a test over the reproducer and a generated family of the class asserts
  work (nodes visited or steps) at most a stated multiple of path length, and fails without the
  fix (`--fails-without`).
- A3 Routes resolve as before: the existing suite and a differential check of the fixed matcher
  against main over the fuzz corpus give identical results.
- A4 Ordinary routes cost no more: an A/B of a typical routing table and path mix at main against
  the fix, with an A/A, as a strict job.
- A5 [artifact] `docs/` note in radix-router naming the class, the bound and the numbers.

## Contracts

None changed: the router's API and route semantics stay as they are.

## Decisions

Exploration assignment: at most three experiments. If the slowness is in the fuzz harness rather
than the matcher, say so and stop.

## May land early

None.
