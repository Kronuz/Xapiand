# placement-without-flush review: correctness

Commit: Xapiand:f3c7d37cb529 (its code outside docs/ equals Xapiand:e7e93fbfa)
Verdict: CHANGES
Date: 2026-11-10

APPROVE: lands as is, at this commit's code. CHANGES: lands after each blocking finding is fixed
and confirmed by a job at the fixed commit. REJECT: the approach is wrong; the quest is re-planned.
An APPROVE lists no blocking finding; a CHANGES lists at least one.

## Evidence I relied on

- build-test 261110-191640-dd5838 and e2e 261110-191642-50ce83, both exit 0 at e7e93fbfa (same src/tests/CMake as
  f3c7d37cb). build-test runs ASSERTS=ON, so `Turn::release`'s `unleased_maps() == 0` assert was live through the
  whole node suite, including the 1- and 2-shard version-race and sequential-model tests.
- Mutants m1 (261110-191418-21153b, published view without refresh) and m2 (261110-191419-e7ea4e, no cross-shard
  look) are both killed by `test_an_existing_id_written_again_lands_on_its_shard`. The segment test fails at master
  (261110-191050-6a255b). Together these cover `written_key` and A1.

## What I checked myself

Probe job 261110-193852-f1d945 at f3c7d37cb, exit 0. Script:
`.scratch/review-placement-without-flush-correctness/run.sh` in the home checkout, with `probe_cases.cc` and
`patch_test_node.py` beside it.

- R1: 20 POSTs with new numeric ids into 5 shards, no commit. Result: 4 4 4 4 4 per shard.
- R2: an off-hash document, committed or still buffered, then DELETE, DELETE, PUT with no commit. Result: 204, 404,
  204. Only the hash shard holds the new document, at v1. A buffered tombstone hides the segment copy.
- R3: 6 threads × 10 unversioned PUTs with no reads or commits, on one off-hash buffered id and one new id. Each id
  ends on one shard (the shard that held it, or its hash shard), at v31 and v30. No write failed.
- M3: `written_count` without `buffered_count()`. Three tests still pass with it: the restore-segments test, the
  existing-id test and `test_document_ids_are_themselves`. R1 catches it: all 20 documents land on shard 0.

Read without a probe:

- Restart: `ShardEndpoint::open_blocking` (src/database/shard.cc:491-501) replays the WAL and commits it inside the
  open, before any turn exists. Any later `written_key` therefore sees the replayed state. This is the same open
  path the old refresh read used.
- Durability: `written_key` can report a write that is buffered and not yet committed. The old read could too: its
  refresh flushed a segment, which is not a commit, and a cancel or fence reopens from the last commit either way.
  The rewrite always goes to the shard that reported the key, so that shard's log orders both writes. The new read
  adds no way for placement and durability to disagree.
- Locks and threads: `written_key` uses `core.index->find_key(key)` (src/engine/strata/shard.cc:1236), which is the
  same call, with the same key, that `replace_by_key`, `replace` and `erase_by_key` make in the turn before
  `apply_t`. It runs in the same task and turn as those writes. The reads add no new lock, thread or lease pattern.
  Each placement read holds one turn at a time (`least_used_shard` and `find_docid_term` release each turn before
  taking the next), so the `*held == 0` assert in `take_turn` holds.
- Visible behaviour: version checks and 409 bodies still come from the write's own turn. The written read only
  picks a shard or a docid. DELETE by numeric id is unchanged, and DELETE by key reads what the old refresh read
  did (confirmed by R2's 404). Nested paths: placement still iterates only the handler's own endpoints, and the
  fix changes only the read made at each one.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | gap | No test checks the `buffered_count()` half of `written_count`. This is the part of least-used placement that differs from the old exact count. Dropping it puts every new numeric-id document of a burst on one shard, and the quest's suite still passes. | Mutant M3 in 261110-193852-f1d945: the quest's tests and the ids test pass; R1 shows 20 0 0 0 0 | Add R1 (`probe_new_numeric_ids_spread_while_buffered` in the probe file) or an equivalent to `tests/test_node.cc`, and show it kills M3 |
| N1 | no | nit | `harness/async_check.sh` reports `async_allowlist.tsv:4: src/database/handler.cc [pool] /lock_shard/ covers 24 of 25 hit(s): lower the count`, because `least_used_shard` folded two loops into one. | log of 261110-191640-dd5838 | Lower the count to 24 |
| N2 | no | nit | The placement reads now always take each shard's turn. The old read took a turn only on a dirty shard. Two effects: the forced-interleaving tests (`test_writers_expecting_one_version_apply_once`, `test_a_versioned_delete_checks_the_document_its_key_names`) now hold writers in the lookup rather than after it, and the "past its lookup, waiting for the turn" comments no longer describe that; and a keyed write fails with `busy` when any shard's turn times out, not only its target's. The tests still pass and results stay correct, because the version check runs in the write's turn and the turn mutex is FIFO. | test_node.cc:1533, :1596; handler.cc find_docid_term | Reword the two comments, or leave as is |
| N3 | no | nit | Older races, not introduced here: an off-hash id racing a DELETE and a PUT can end on two shards, because the lookup and the write are not atomic across shards; and `upload_stored` puts the blob on the hash shard while `index_uploaded` finds the shard that holds the id. Both were the same with `get_docid_term`. | handler.cc:1937-1947, :560-566 | None in this quest |

## Not run

TSan. The new reads use the write path's own lease-less `find_key` under the same turn and add no thread or lock,
and build-test ran with asserts on. No TSan build of this code exists. ASan+UBSan was not run either. A crash or
restart probe was not run: the open replays and commits before any turn, which I settled by reading the code. A2
timing (jobs d043da and 460eb8) is outside this lens.
