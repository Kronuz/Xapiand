# one-id review: durability

Commit: 8d61b1913d6204d431622d4443446667d9e54380
Verdict: APPROVE
Date: 2026-11-13

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- 261113-083755-c5e4b4 build-test at this commit (A1, A2, A4, A6, A7): the suite, including
  test_the_id_counter_persists_replays_and_advances (test_pool), test_ids_route_by_bucket,
  test_a_dump_and_restore_keeps_every_id, test_far_ids_keep_the_docids_dense (test_node).
- Mutants 261113-083755-2de9cb .. 261113-083809-734db4 (tests/one_id_mutants.sh): no-persist,
  no-replay, no-advance, no-stride, hash-route, named-docid each fail the test they target.
- CI green at Xapiand:3d968d72c (sanitizers), per the lead.

## What I checked myself

Scripts in `.scratch/review-one-id-durability/` (probe.sh patches tests/test_pool.cc in the job's
checkout, builds xapiand_test_pool, runs the probes, restores the file).

- Probe job 261113-095152-b20016 (261113-094131-fb3a43 was my compile error):
  - P1: issue 1, commit, PUT explicit 10 logged but not committed, the shutdown's commit fails on
    a sync fault, reopen: 10 is replayed and the next automatic id is 11. Passes.
  - P2: the same with the stored `_id` as a map `{"_value": 10}`: 10 is replayed, the next
    automatic id is 2. Fails, as N1 says.
- Counter durability, by reading. The counter is written as `set_meta` in the same prospect
  transaction stream just before `eng.commit` (shard.cc:714-718; strata shard.cc:1120-1140), and
  prospect makes applied ops durable only at `commit()` (strata index.hh:952-967). So `_id_high`
  becomes durable at exactly the commit point of the documents that raised it. do_commit,
  do_replace_term, apply_slice and written_load all run in the index's turn, so no write lands
  between the `set_metadata` and the `id_high_moved_ = false` that clears it. The only other engine
  commits (commit_blocking:584, settle_merges:691) are gated on `merge_output_unsynced()`, which
  requires `applied_since_commit_ == 0` (index.hh:865), so they never commit documents without the
  counter.
- Crash points. Before the log push: `issue_id` and `raise_id_high` run before
  `storage_push_blobs`; a failure leaves a raised in-memory counter and no document, which is
  harmless. After the push, before commit: the WAL line carries the issued `_id` (issue_id runs
  before the push, shard.cc:885), the replay raises from it (shard.cc:457-460), and the suite plus
  the no-replay mutant prove it. Mid-RESTORE: the slices go through `do_replace_term` with the WAL
  on (shard.cc:1488), so an explicit id raises live and on replay (P1). A duplicate id is never
  written in any case: an issued id writes with expected version 0 (shard.cc:904-906, R3), so a
  counter that fell behind fails with a conflict and takes the next id.
- Routing. GET, PUT of an existing id, DELETE and RESTORE of a found id all locate the document
  in any shard (find_docid_term). Only new placements route, all through `id_shard`: PUT
  (handler.cc:2021), bulk and RESTORE slices (:2643), upload_stored (:659), delete of a missing id
  (:1920), and UUID or keyword automatic ids (schema.cc:2789, :2854). Automatic numeric ids come only
  from buckets the issuing shard owns (next_auto_id), so `id_shard(issued key) == issuing shard`.
  For float-typed ids, numeric_id decodes the float key to the same N (the stride test checks
  this). Routing is stateless given n, so a reopen or RESTORE cannot move an id. A3: grep of src/
  for `numeric_docid|new_did|NumericId|replace_docid` and `% n_shards` leaves only hit-handle
  docids (`(did - 1) % n`), which the design note lists as handles, never ids.
- R7. `open_blocking` runs only from the catalog's `open_payload` (pool.cc:591-594), on a
  WorkPool worker. With `host = true`, `refresh_for_read` does nothing (strata shard.cc:313-316).
  `metadata()` acquires a reader and finds the key without a `co_await`, so `sync_wait` returns at
  once. It cannot block or reach the loop. The worst case is a throw (too_many_readers, or a
  malformed value), which fails the open loudly.
- Mutant coverage, by reading. A set_metadata moved after `eng.commit` would be killed by the
  test_pool counter test: the clean shutdown skips the commit when `modified` is false, so the
  reopen issues 1 again and conflicts. A mutant that ignores the floor is likely killed by
  test_a_dump_and_restore_keeps_every_id at 3 shards: without the floor, `highest > 1000` fails,
  since the empty shard 2 would issue 3. I did not run either mutant.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| N1 | no | gap | The live write raises the counter from the key (`numeric_id(key)`, shard.cc:918); the WAL replay raises it from the stored `_id` (`stored_numeric_id`, shard.cc:361-380). These are two sources for one fact. When a stored `_id` is not a scalar integer or float, for example a `{"_value": N}` map, a replayed write does not advance the counter, and a later POST is issued below an existing id. R3 rules out an overwrite. I did not verify whether a server request ever stores `_id` in that shape. | Probe P2, job 261113-095152-b20016: id 10 is replayed and the next automatic id is 2. | Raise from the replayed document's key once engine::Document exposes it (R6 deferred this), or normalise the stored `_id` to its scalar. Optionally keep it as is and state the assumption in docs/design/one-id.md. |
| N2 | no | nit | No suite test covers the replay of an explicit or restored numeric id that the last commit missed. The counter test replays only automatic ids, and the restore test commits. The code is right (P1). | Probe P1, job 261113-095152-b20016 (passes) | Adopt P1 (`.scratch/review-one-id-durability/probe_tests.cc`) into test_pool. |
| N3 | no | nit | `open_blocking` turns a non-integer `_id_high` into a throw, which fails the open. Users cannot write the key (search_views.cc:1195), so only corruption reaches this. | shard.cc:522-523 | None needed. |

## Not run

- The floor mutant and the set_metadata-after-commit mutant (reasoned above, not run).
- Whether a real request can store `_id` as a map (N1's reach).
- Whether the replay's own commit inside `open_blocking` (shard.cc:548, before this quest) fits
  D60c. one-id adds only a read to the open.
- Sanitizers, other platforms and A5 (CI's and the lead's jobs).

## Re-check

Commit: 1f5b791d21db8e7a39f78432d8a62f096a511960
Verdict: APPROVE
Date: 2026-11-13

Scope: `git diff 8d61b1913 1f5b791d2 -- src tests`. That covers routed-first `find_docid_term`, two-pass RESTORE placement,
`key_reads`, test_a_lookup_by_id_reads_one_shard, the test_loop_yield.cc:952 count and the lookup-fan-out and bulk-fan-out mutants. I
relied on build-test 261113-103109-652819, the mutant jobs and CI at this commit, as the lead reports them, and did not
rerun them. I made no probe build. Everything below was checked by reading.

Question: can routed-first miss an id that exists, so that a PUT or RESTORE places a second copy? No.

- `find_docid_term` (handler.cc:1826-1830) visits `routed, then 0..n-1 without routed`. That order is a
  permutation: for each `i >= 1` it maps `i <= routed ? i - 1 : i`. On a miss it still reads every shard, and it stops at the first hit, as
  master does. Only the order changed.
- A copy can appear only if the id exists in some shard Y at the moment the write lands somewhere else. After one-id every
  create goes to `id_shard`'s shard X. An off-route document in Y (R8: written before one-id, or straight to a shard)
  can therefore only be replaced in place or deleted, and no new one can appear. A lookup that reads Y while the document is there finds it.
  If a DELETE removes it before Y is read, the miss is correct, and the PUT creates the only copy in X. If the DELETE lands after the
  hit, the in-place replace runs with `must_exist` (handler.cc:566-573, apply_slice's `do_replace(..., true)`), answers
  `doc_not_found`, and the document is placed afresh in X. Two racing PUTs of a new id both go to X, where
  `replace_by_key` in X's turn makes the second one an update. In no interleaving do X and Y both hold the id.
- RESTORE placement (handler.cc:2544-2603) asks each term once per shard over the two passes: pass 1 asks
  only the routed shard (`(id_shard == shard) == routed`), and pass 2 asks the others. A pin or read failure in either
  pass marks the asked terms serial, and the serial path does its own full `find_docid_term`. `continue` replaced
  `break`, so a shard with nothing to ask no longer ends the loop early. A write racing pass 2 can only create the id in
  X. The slice then writes it through `do_replace_term` in X, keyed, so that write is an update. A found document whose copy vanished is
  `missed` and placed afresh (B1).

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| R1 | no | gap | The fallback that R8 exists for, finding an off-route id on a miss, has no test. Both new mutants check only that a hit reads one shard. A routed-only mutant (3c328c6e2's behavior) is probably not killed, because no test writes an id off its routed shard. | handler.cc:1826-1830; tests/one_id_mutants.sh lookup-fan-out and bulk-fan-out | Add a test that writes an id straight to a non-routed shard endpoint, then checks that GET finds it and that a PUT and a RESTORE replace it in place (one copy, version 2). Pair it with a routed-only mutant. |
| R2 | no | nit | An `is_missing` pin, meaning a shard index that is absent, ends the lookup. Callers treat that as not found and place the document afresh. Because the routed shard is now read first, an absent routed shard skips the reads of the shards after it, where master skipped only shards above the absent one. This is the same class of behavior as master, reachable only with a shard directory missing and off-route ids present. | handler.cc:1830; pass 1 `missing = true` at :2597 | None needed; note in the design note if wanted. |

Not run: probes of R1 and R2 (no build), and the performance side of `key_reads`.
