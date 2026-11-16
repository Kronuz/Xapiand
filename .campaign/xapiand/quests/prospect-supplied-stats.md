# prospect-supplied-stats: Weights from supplied statistics

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine
state (status, branch, checks, reviews) is in `state.json`: `campaign status
prospect-supplied-stats`.

## Outcome

prospect's searcher accepts collection statistics from its caller (document count, total length,
per-term document frequency and collection frequency) and weights with them instead of computing
them from its own snapshot (`searcher.hh:104-151` today). With no statistics supplied it behaves
exactly as now. Xapiand's distributed search (quest `distributed-search`, design D10) gathers each
index's statistics across nodes in a `stats` round trip and passes them here, so a shard's scores
equal one node's holding every shard.

## Acceptance criteria

- A1 On the release-rows corpus split over 3 and 5 shards, weights and result orders with supplied
  statistics equal those computed from one index holding the shards' merged content (scores
  bit-equal, orders equal), for BM25 and every weight prospect ships: a test with values, run as a
  job
- A2 A mutant that ignores the supplied statistics fails A1 (`--fails-without`)
- A3 No statistics supplied: every existing prospect test and `verify` passes unchanged, and query
  throughput on the Phase 4 rows is within the A/A band of `main` (one short before/after
  `exclusive` job)
- A4 [artifact] `docs/mechanisms/supplied-statistics.md`

## Contracts

Provides `prospect:supplied-stats`: an optional statistics input on the searcher (a C++ API in
`include/prospect/searcher.hh`), per index, with the term statistics keyed by field and term.
Consumed by `distributed-search`.

## Decisions

- Design D10: statistics stay per index; the batch key changes in Xapiand, not here.

## May land early

All of it: the input is optional and changes no result when absent.
