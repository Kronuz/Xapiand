# agg-shard-parity: Xapiand's aggregations equal over one shard and many for every kind it serves

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status agg-shard-parity`.

## Outcome

Xapiand master serves aggregations from its own `src/aggregations`, not prospect's collectors, so
release-rows' A3 (prospect's 16 kinds) does not cover the release. A Xapiand-level test indexes one
corpus into one shard and into several, runs every aggregation kind Xapiand serves, and compares
values (not counts) between the two; `_terms` past `_shard_size` follow the release-rows ruling.

## Acceptance criteria

- A1 Every aggregation kind Xapiand serves returns equal values over one shard and over several (1, 2, 5 shards), with deletes: a test in the build-test check
- A2 The test fails when a shard's partial is dropped from the merge: a `--fails-without` job
- A3 [artifact] The kinds covered, and any that differ by design: `docs/dev/agg-shard-parity.md`

## Contracts

None.

## Decisions

- Lead, 11-11: split from release-rows A3 (review F1).

## May land early

Everything.
