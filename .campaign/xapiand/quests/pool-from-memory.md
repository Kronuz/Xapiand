# pool-from-memory: The open-index pool is sized from memory, not a fixed 200

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status pool-from-memory`.

## Outcome

With no `--database-pool-size`, the pool keeps open as many indexes as 1% of physical RAM holds at the measured cost of an open index (~40 KiB; ~6,000 on 16 GiB), bounded by the descriptor budget `adjustOpenFilesLimit` computes; an explicit size still wins (D89).

## Acceptance criteria

- A1 The default follows RAM and the descriptor budget: a unit test feeds RAM and descriptor limits and checks the pool size, with a mutant restoring the fixed 200
- A2 2,500-index search at the default pool on a 16 GiB host: p50 within the A/A band of `--database-pool-size 5000` (132 ms at 0d97b2da7): a strict job before and after
- A3 The suite passes and CI is green at the landed code
- A4 [artifact] The configuration docs state the default and how it is derived: `docs/src/**` (the page naming `--database-pool-size`)

## Contracts

Changes the default of `--database-pool-size`; the flag and its meaning stay.

## Decisions

<Decisions made, and by whom; open questions, naming the owner or the lead.>

## May land early

<Internal code behind tests, unless the quest is a format, wire or public API. "None" when
everything waits for the review; `land --upto` refuses then.>
