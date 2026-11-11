# placement-without-flush: New documents are placed without flushing every shard

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status placement-without-flush`.

## Outcome

Placing a new document in a multi-shard index (`src/database/handler.cc:1913-1933`: the doc count
for new numeric ids, `get_docid_term` at :1739-1746 for keyed ones) reads each shard without
flushing its writer: the lookup sees unflushed documents through strata's writer-side
`basic_index::find_key` (index.hh:1689) and `buffered_count()`, instead of `engine_for_read` ->
`ShardEndpoint::fresh` (pool.cc:361) -> `Index::refresh()`, which writes the buffer out as a new
segment per document. A 1,000-document RESTORE into 5 shards then makes a few segments per shard,
not about 226, and no longer stalls behind another process's fsyncs (explore/restore f01ae3583:
50-60 s beside an fsync storm against 2.4 s alone).

## Acceptance criteria

One line each: `- A1 <criterion>: <the job, test or check that shows it>`. An untagged criterion
needs a job that passed at the landed code; tag one `[artifact]` (a committed `path`),
`[decision]` (an id in PLAN.md's Decisions table) or `[manual]` (checked by hand) only when a job
cannot show it.

- A1 A 1,000-document RESTORE into a 5-shard index leaves at most a few segments per shard (the test reads the shards' segment counts) and an existing id written again lands on the shard that holds it, unflushed or not: tests in `tests/test_node.cc`, in the `build-test` check, failing at master
- A2 RESTORE /bank/ and /cities/ beside `harness/probes/fsync_storm.py` finish under 10 s each, against 50-60 s on master: a `strict` job, before and after
- A3 The rest of the suite and the docs E2E pass: the `build-test` check and the e2e run
- A4 [artifact] `docs/mechanisms/` names how placement reads a shard without flushing it

## Contracts

No API or format change. Placement keeps its rule (least used shard, or the shard that holds the id); only how it reads changes.

## Decisions

- Lead: found by the restore explorer (explore/restore f01ae3583). Its experiment skips the cross-shard check for existing ids and is not landable; the quest keeps that check.
- Risk high: write-path placement decides which shard holds a document (correctness lens).

## May land early

The tests and the probe, before the fix.
