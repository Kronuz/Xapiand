# one-id: journal

State: waiting on jobs 261113-074628-298443 261113-074720-30a477 261113-074651-a42351 261113-074654-a175a2 261113-074657-794e8b 261113-074700-cc13f7 261113-074703-8f82e0 261113-074707-55907d 261113-074710-bc8074 261113-074808-b39d85 261113-074731-77e865 261113-074811-d08bbf
Quest: .campaign/xapiand/quests/one-id.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Waiting on jobs at 622f57d08 (tests and mutants committed; user docs and the ids mechanism note updated):

- 261113-074628-298443 probe: the new tests and engine placement tests (not evidence).
- 261113-074720-30a477 build-test, tagged A1, A2, A4, A6, A7.
- Mutants (`tests/one_id_mutants.sh`, --fails-without, after the probe): A1 named-docid 261113-074651-a42351;
  A2 no-persist 261113-074654-a175a2, no-replay 261113-074657-794e8b (test_pool), no-advance 261113-074700-cc13f7,
  no-stride 261113-074703-8f82e0 (test_node); A7 hash-route 261113-074707-55907d; A6 no-advance 261113-074710-bc8074.
- A5: release builds master 261113-074808-b39d85, head 261113-074731-77e865 (into .scratch/rel-<sha>), then the
  exclusive ABBA 261113-074811-d08bbf (.scratch/ab/ab.sh, the loop-scheduling A2 harness with arms pre/cur).

## Next

1. Read the probe; fix any test; when green, paste `campaign brief`'s table, push the branch for CI (A4 sanitizers).
2. A5 numbers into the Result (light p99 loaded, heavy searches, cur/pre per pair; a miss stated as a miss).
3. Correctness review is the lead's to launch.

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
