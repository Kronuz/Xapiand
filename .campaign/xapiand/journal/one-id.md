# one-id: journal

State: working
Quest: .campaign/xapiand/quests/one-id.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Tests for A1, A2, A6, A7 (tests/test_node.cc, tests/test_pool.cc) and the mutant script; then the build-test job.

## Next

1. Write the tests and tests/one_id_mutants.sh; submit build-test (A4) and the per-criterion jobs with mutants.
2. A5: one exclusive job, master vs head, harness/stall_load.py T4/T5 (docs/perf/x7-loop-stalls.md).
3. User docs: docs/src index-api.md and architecture.md routing paragraphs.

## Blocked on

msg-44 to the lead (asked, proceeding): src/database/pool.h is outside the file set; ShardEndpoint, declared
there, holds the id counter (two members and `raise_id_high`).

The bucket-to-shard table is not persisted (design note, "Open"): `bucket % n` until the reshard quest. Options:
(a) persist it now in the index settings beside number_of_shards (format and plumbing with no reader that could
differ); (b) persist it with the reshard quest. Recommendation: (b).

## Evidence

| Criterion | Evidence |
|---|---|
| A3 | `docs/design/one-id.md` |

## Result

<pending>

## Rulings I made

- R1 The id counter is one high-water mark per shard (`_id_high` metadata), not 1024 stored counters: every bucket's
  counter is derived from it, which is what keeps automatic ids above every numeric id present (contract) while D85's
  per-bucket stride holds. Equivalent under reshard (move a bucket: raise the target's mark).
- R2 Auto ids are also above every other shard's counter: `least_used_shard` already takes one turn per shard, and
  now reads each counter there and passes the largest as a floor (DB_SLOT_SHARDS third value).
- R3 An automatic id is written with expected version 0 (must not exist): a counter that fell behind conflicts,
  never overwrites.
- R4 Numeric ids with buckets are integers 1..2^53-1 (exact as a double key); others hash. Was 1..2^32-2 (docid range).
- R5 `DatabaseHandler::replace_document(string_view)` (`&N` creating at a caller's docid) removed: no callers.
- R6 WAL replay raises the counter from the replayed document's stored `_id` (engine::Document has no key getter, and
  src/engine/document.h is outside the file set).
