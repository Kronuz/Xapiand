# journal-bound-first-open: the lost bound survives a power cut during the first open

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status journal-bound-first-open`.

## Outcome

Closes journal `docs/mechanisms/lost-bound.md`, Limits, second item (turn-log-parity durability
review, N1): after rule-5 damage, the first open writes its new epoch's open record before
`Host::open` stores the bound or the loss; a power cut between the two makes the next open report
no damage, so acknowledged records the damage took are silently gone. Power cuts at every mutation
of that first open disagree at 11 of 17 cuts (jobs 261121-141538-9a63b7, 261121-141627-fe334f).
After this quest the verdict is durable before the open record, so every cut either reports the
damage with its bound or the loss.

## Acceptance criteria

- A1 A power cut at every mutation of the first open after rule-5 damage: the next open reports the
  same bound or loss at all 17 cuts (a test run by the suite)
- A2 The test fails without the fix (a `--fails-without` job)
- A3 [artifact] `docs/mechanisms/lost-bound.md` drops the limit and says how the verdict is stored

## Contracts

Journal recovery and the group table; Detent's lead reviews the diff against its authority suite
before landing. Lands after Detent's `journal-earlier-turn-loss` (same recovery path).

## Decisions

- Owner, 2026-11-23: turn-log-parity lands with this as a recorded limit; this quest fixes it.

## May land early

Nothing: one durability fix, reviewed once.
