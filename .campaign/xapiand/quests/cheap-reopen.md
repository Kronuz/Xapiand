# cheap-reopen: Reopening an evicted index costs near a cached open

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status cheap-reopen`.

## Outcome

Reopening an index the catalog evicted costs close to finding it open: a search across more indexes than the pool holds stays within a small factor of one that fits (D89). The design picks the mechanism (cached metadata or manifest kept past eviction, lazy reader opens, or what the measurement shows), in strata where it belongs, not in Xapiand.

## Acceptance criteria

- A1 [artifact] Design note with the measured reopen breakdown and the chosen mechanism: `docs/design/cheap-reopen.md`
- A2 2,500-index search at a pool of 200: p50 within 2x of the pool-5000 run at the same commit (884 vs 132 ms at 0d97b2da7 today): strict jobs before and after
- A3 Durability unchanged: the crash and recovery checks pass, and a reopened index serves exactly its last commit: the suite plus a reopen-after-crash test
- A4 The suite passes and CI is green at the landed code

## Contracts

Consumes strata's catalog and open path; any strata change is its own quest in strata ordered `after` this design.

## Decisions

<Decisions made, and by whom; open questions, naming the owner or the lead.>

## May land early

<Internal code behind tests, unless the quest is a format, wire or public API. "None" when
everything waits for the review; `land --upto` refuses then.>
