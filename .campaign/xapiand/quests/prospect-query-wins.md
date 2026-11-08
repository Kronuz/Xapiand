# prospect-query-wins: Counted-union pruning and required-optional block skipping

Risk: normal. History: this landed before the campaign, under the earlier campaign skill, at prospect:e20499a (2026-10-20); its ledger `docs/status/query-wins.md` in Kronuz/prospect is the record. The quest's repository is `../prospect`. Its machine state is in `state.json`: `campaign status prospect-query-wins`.

## Outcome

Rows 1 and 3 of the loss attribution are production code: counted unions of three or more clauses count from window bits and prune the scoring, and intersection_union skips blocks as Lucene's ReqOptSumScorer does.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] Q1 results identical to main (dumps at 100k and 1M, merged and reordered, nine 100k segments; verify 0 mismatches): `docs/status/query-wins.md`
- A2 [artifact] Q2 parity 419 of 419, worst error 3.87e-16: `docs/status/query-wins.md`
- A3 [artifact] Q3 each row reverted fails search_test's tally; five mutants caught: `docs/status/query-wins.md`
- A4 [artifact] Q4 ASan and UBSan over the suite; CI linux-asan-ubsan green: `docs/status/query-wins.md`
- A5 [artifact] Q5 run-level A/B at 1M, 16 rounds, A/A first: no row lost; TOP_100_COUNT union of 3 0.461 and 0.390: `docs/status/query-wins.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Lucene's finer windows were measured slower and not taken (3b5a311).
- NOTICE and the Apache License added for the Lucene port.
- Risk normal: changes which documents top-k scores, identity-checked (the auditor's mapping).

## May land early

None.
