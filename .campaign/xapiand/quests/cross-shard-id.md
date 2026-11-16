# cross-shard-id: A concurrent PUT never leaves an id on two shards

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status cross-shard-id`.

## Outcome

A PUT that found an off-hash id on shard A writes there only if the id is still at that docid
(`WriteArgs::must_exist`); otherwise it places the document afresh, so a concurrent DELETE and PUT
never leave the id on two shards. Prototype and repro: branch `explore/cross-shard-id`,
`docs/dev/cross-shard-id.md` (before: 261111-073927-6d9b0b, id on both shards).

## Acceptance criteria

- A1 The held-turn test in `tests/test_node.cc` (PUT finds X on A, DELETE erases it, PUT writes on
  H, the first PUT resumes) leaves X on exactly one shard, with GET and the SEARCH total agreeing;
  tagged `--fails-without` on the fix reverted.
- A2 The DELETE in that test replies with its real status: the empty reply (status 0) seen at
  508d1192a (261111-073929-a46f52) is explained and asserted, never dropped from the test.
- A3 `Shard::replace` with `must_exist` answers doc_not_found when the key is not at the docid, and
  every caller that passes it (`replace_document_term`, `index(did)`) handles that answer; a test
  per caller.
- A4 The suite passes at the head, with CI green.

## Contracts

`WriteArgs::must_exist` in `src/engine/shard.h`: an internal engine-seam flag, no wire change.

## Decisions

Lead: one correctness review with the concurrency questions; the fix starts from the explore
branch's prototype.

## May land early

The held-turn test, disabled until the fix.
