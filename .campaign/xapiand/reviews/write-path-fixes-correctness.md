# write-path-fixes review: correctness

Commit: 60a5e373643b8253c7a91e608d7252eda46d180a
Verdict: CHANGES
Date: 2026-11-10

APPROVE: lands as is, at this commit's code. CHANGES: lands after each blocking finding is fixed
and confirmed by a job at the fixed commit. REJECT: the approach is wrong; the quest is re-planned.
An APPROVE lists no blocking finding; a CHANGES lists at least one.

## Evidence I relied on

- 261110-045411-83e91d (build-test at 60a5e3736, exit 0): A3 existing suites and A5 local build.
- 261110-045430-4ed4e9 (engine strata and node tests at 60a5e3736, exit 0): A1 and A2 on a
  single-shard node (the test node runs `--shards 1`, tests/test_node.cc:241).
- 261110-045423-57a7f5 (docs E2E, exit 0): A3.
- 261110-045424-bc00ed (TSan, exit 0): A4 on one shard.
- The base and mutant jobs in the journal's "Fails without the fix" section, read as job records:
  enough for "fails at master" for the single-shard tests.

These cover one shard. The node default is 5 shards (src/opts.cc NUM_SHARDS), so I probed the
multi-shard paths and WAL replay myself.

## What I checked myself

Probe job 261110-055943-9ecd3c at 60a5e3736 (exit 1, from the failures below). Scripts are in
`.scratch/review-write-path-fixes-correctness/`: `run.sh` (the job), `patch_test_node.py` (adds
`probe_cases.cc` to tests/test_node.cc and reads `--shards` from `PROBE_SHARDS`), and
`wal_probe.sh`. To rerun at a fix, submit the same `run.sh` with `--commit <fix>`.

- **P1, versioned DELETE racing a docid reuse.** Setup: hold the turn of every shard. Queue
  `DELETE /dr/-1`, then `PUT /dr/<g>` (g is the global docid of -1, so the new id takes the freed
  docid), then `DELETE /dr/-1?version=1`; then release. Results:
  - 1 shard: 409 `_version` 0, and the other document is kept.
  - 2 and 5 shards: the versioned DELETE answered **204** and **deleted /dr/g**. See D1.
- **P2, writers expecting one version, with every shard's turn held.** Tested PUT, PATCH, UPDATE,
  UPSERT and DELETE, five ids each, six writers, at 1, 2 and 5 shards. Every time exactly one
  write won, the rest got 409 with `_version` 2 (0 for DELETE), and the document held the winner
  at version 2 (270 checks per run). This answers risks 1 and 4 for every method except
  multi-shard DELETE, which is D1.
- **The quest's own node cases at 2 and 5 shards.**
  - The precondition case passes (111 checks).
  - The sequential model passes ("48 won, 41/42 refused").
  - The ids round trip passes every id check. It fails one check, `posted _id == "3"`
    (tests/test_node.cc:1268), which assumes a single shard. See N1.
- **P3, WAL replay after kill -9 before any commit, at 1 and 5 shards.** Accepted writes:
  versioned PUT, PATCH, `PUT /w/0?version=0` and `DELETE /w/-1?version=1`. Refused writes: PUT,
  PATCH and DELETE with stale versions, and a version given for a missing id. After the restart:
  - `/w/1` was at v3 and `/w/0` at v1.
  - `/w/-1` was gone, `/w/2` was untouched and `/w/3` was absent.
  - `PUT /w/1?version=3` then landed at v4.

  So no refused write was re-applied and no accepted write was lost or refused. The one "FAIL"
  line is my script expecting 200 where PATCH answers 204.

  The code agrees. A refusal throws before the WAL line (src/database/shard.cc do_replace,
  do_replace_term, do_delete*). Replay runs `do_replace(version=false)` and `do_delete` with no
  expected version (src/database/wal.cc:546-551, src/database/shard.cc:416-427). A placed write
  is logged at its docid, and replaying it lands at that docid.
- **Code read.** The engine-side checks are in the turn in Shard::replace, replace_by_key, erase
  and erase_by_key (src/engine/strata/shard.cc:867-1010). The id-0 cause and its fix are
  NEW_NUMERIC_KEY, has_document_id and numeric_docid (src/database/handler.h,
  src/database/schema.cc:2807-2984).
- **Risk 5, writes without a version.** PUT and DELETE carry no expected version.
  PATCH/UPDATE still use the read version as a retried CAS, as before. The only unversioned
  change is that a replace by docid whose key is missing and whose docid holds another document
  now retries instead of overwriting it, which fixes a defect.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| D1 | yes | defect | **Multi-shard DELETE checks the version against the wrong document.** Before taking the turn, the handler resolves the key to a docid, then erases by that docid. In the turn, Shard::erase compares `?version` with whatever document is at that docid, not with the document the key names. If the keyed document is deleted and its docid is reused before the turn, `DELETE ?version=N` removes the other document and answers 204. The new placement makes this reuse ordinary: an in-range numeric id takes a freed docid. A recreated keyword document gets a 409 with `_version` 0 even though it exists. The unversioned multi-shard DELETE has the same wrong-document race, which predates this quest. Shards=5 is the node default. | P1 in 261110-055943-9ecd3c: 204, and the other document deleted, at 2 and 5 shards; correct at 1 shard. Code: src/database/handler.cc:1807-1831 (delete_document_term with n_shards > 1 calls delete_document(did)), src/engine/strata/shard.cc:965-980 (erase checks version_of(did)) | In the multi-shard branch, use the found docid only to pick the shard (`(did - 1) % n_shards`), then erase by key there: `endpoints[shard]` → `delete_document_term(term, ..., expected_of(document_ver))`. The check then runs in erase_by_key in that shard's turn, the same as one shard. Add P1 (or an equivalent case) at 2 shards to the suite. |
| G1 | no | gap | Every node test of ids and versions runs on one shard (`--shards 1`, tests/test_node.cc:241), but nodes default to 5. The multi-shard routing added here (numeric_docid shard choice, DB_SLOT_SHARDS on every key, new_did per shard) has no test in the suite. My P2 and the 2- and 5-shard reruns pass, apart from D1 and N1. | 261110-055943-9ecd3c | Run the id, precondition and apply-once cases at 2 shards too, for example by parameterising RunningNode the way `patch_test_node.py` does. |
| N1 | no | nit | `test_document_ids_are_themselves` expects the POST after `PUT /ids_seq/2` to get id 3. Multi-shard, a POST goes to the least-used shard, so its id differs. The checks that matter (2 untouched, still at version 1) pass. | tests/test_node.cc:1268; 261110-055943-9ecd3c at 2 and 5 shards | Check `_id != "2"` instead of `== "3"`, or keep the exact check only for one shard. |
| N2 | no | nit | PUT by docid still answers errc::invalid (500, "The key belongs to another document") when the document is deleted and recreated at a new docid between the handler's lookup and the turn. Nothing is applied, but a versioned writer should get a 409. The path predates this quest. | src/engine/strata/shard.cc:857-864, src/database/handler.cc:500-531 | Map that case to version_conflict so it retries (unversioned) or answers 409 (versioned). Or leave it for later. |
| N3 | no | nit | An explicit id of 2^32-1 now takes docid 2^32-1 (as it did on master). After that, `last_docid() + 1` wraps to 0 for every POST or by-key write in that shard. This is not new, but this quest widens which ids land at docid = id. | src/database/handler.cc numeric_docid (`id <= last`); src/engine/strata/shard.cc:819, 929 | Cap numeric_docid below the largest docid, or refuse a write once the shard's docids are exhausted, in a later quest. |
| N4 | no | nit | `has_document_id` treats a boolean `false` (and any map or array) as an id. Under the old MsgPack truthiness, `false` meant "no id". | src/database/handler.cc has_document_id | Probably harmless. Refuse non-scalar ids if wanted. |

## Not run

- TSan or ASan on the multi-shard paths: P1 and P2 ran in a RelWithDebInfo build with asserts on.
- CI on other platforms: the branch is not pushed.
- Whether the kill -9 in P3 always beat the 1 s autocommit. The post-restart state is right
  either way, but I did not separately confirm that replay rather than a commit produced it.
- Bulk and RESTORE writes with versions: they pass UNKNOWN_REVISION by design.
- Ids past 2^53 sharing a key (ruling 6): not probed, and outside the quest.
