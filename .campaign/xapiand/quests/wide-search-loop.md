# wide-search-loop: A wide search never stalls the HTTP loops, alone or concurrent

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status wide-search-loop`.

## Outcome

A search across thousands of indexes yields the HTTP loop like any heavy request, and concurrent wide searches are bounded: light requests keep their p99 beside them. From x7-gate SR3 (49-100 ms stalls at 2,500 indexes, 393 ms at 10,000) and security S2 (64 clients: light p99 86.9 ms, max 1.95 s; explicit name lists bypass the walk gate).

## Acceptance criteria

- A1 [artifact] The stall attributed (per-name resolution, per-index work) and the bound chosen: `docs/design/wide-search-loop.md`
- A2 SR3: every HTTP loop's max lag over the timed searches at 2,500 and 10,000 indexes within T4's slice: strict jobs before and after
- A3 S2: `wide_starve.py` (security review probe) at 64 clients: light GET p99 within T5's bound: a strict job before and after
- A4 The suite passes and CI is green at the landed code

## Contracts

<What this quest provides or consumes (APIs, formats, protocols), stated precisely enough that a
quest depending on it can start before it lands.>

## Decisions

<Decisions made, and by whom; open questions, naming the owner or the lead.>

## May land early

<Internal code behind tests, unless the quest is a format, wire or public API. "None" when
everything waits for the review; `land --upto` refuses then.>
