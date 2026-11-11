# cross-shard-id review: correctness

Commit: Xapiand:3ee633ad1 (quest/xapiand/cross-shard-id; the code of Xapiand:100856fca plus the master merge)
Verdict: APPROVE
Date: 2026-11-11

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- 261111-090543-e64abf (head code, Xapiand:100856fca): the four new node tests pass (18, 11, 15 and 15 checks).
- 261111-090603-c42e4e: the suite at the head (A4); CI green at the branch per the lead.
- Fails-without reruns, all `done`, exit 0 (the test fails with its part of the fix removed): 261111-112504-02634b
  (A1, engine ignores must_exist), 261111-112508-23ceb1 (replace_document_term test), 261111-112513-f0ef79 (engine
  must_exist test), 261111-112517-58c509 (version check removed), 261111-112521-bb17c1 (A2, DELETE joined early).
  The earlier logs (261111-090546-c325c5, -090549-756aed, -090551-a199d7, -090554-6173c1, -090558-1554c8) show
  which check each mutant breaks: the off shard still holds the id, the GET body and SEARCH total disagree, the
  engine answers differ, the PATCH overwrites the new document (`n == "new"` fails), and the DELETE is 0, not 204.

## What I checked myself

Read: every path from a lookup to a write by docid (`index(did)` for PUT, PATCH, UPSERT/UPDATE, upload;
`replace_document_term` for the bulk indexer/RESTORE and the fallback), the engine's `Shard::replace`, `do_replace`,
WAL replay (`ReplayTarget::replace_document`, must_exist false: it applies only logged, applied writes), and every
other engine::Shard implementation (strata is the only one).

1. Two copies or none (risk 1). After the fix, a write of a key to a shard other than its placement shard happens
   only while the key is at that docid (src/engine/strata/shard.cc:869); every fresh placement goes to the placement
   shard by key (`replace_by_key`, serialized by that shard's turn). So an off-hash copy can only disappear, never
   reappear, and a lookup that misses it on its shard means it is gone for good; two fresh placers land on one shard
   and one key. A must_exist failure writes nothing and skips `finish_write`, so nothing is applied twice. The only
   other docid writes are replay and the explicit-docid `replace_document(string_view)`, which places by docid
   anyway. Probe P2 below found no interleaving that breaks this.
2. Versions (risk 2). PUT builds a fresh `Data`, so an unversioned PUT carries no version slot and places afresh;
   PATCH and UPDATE carry the version they read, get `version_conflict(0)` from the check at shard.cc:871, and their
   loops read again. `?version=0` passes the check and the fresh placement enforces "absent". Probe P1 shows an
   explicit `?version=1` PATCH whose document vanished and was recreated at version 1 answers 409 and leaves the new
   document as it was; the PATCH test covers the implicit version (patches the new one, at version 2).
3. Callers (risk 3). Both callers that pass must_exist (`index(did)`, handler.cc:474; `replace_document_term`,
   handler.cc:1950) catch `is_missing` and place afresh; the retry loops only repeat on version_conflict, at most
   CONFLICT_RETRIES times, and the fallback in `index(did)` looks the id up again, so it cannot loop. No caller maps
   doc_not_found to the client. A behavior change on a single shard: a stale write whose key moved to another docid
   used to answer `invalid` ("The key belongs to another document"); now a PUT is placed by key and a PATCH retries.
   That is an improvement.
4. Tests assert values (risk 4): which shard holds the id (`holds` on both shards), GET bodies, SEARCH total,
   DELETE 204, the PATCH's `_version` 2, the engine's errc per case.

Probe job 261111-113122-7c9756 at Xapiand:3ee633ad1 (exit 0), scripts in
`.scratch/review-cross-shard-id-correctness/` (`run.sh` inserts `probes.cc` into tests/test_node.cc with
`insert.py`, builds xapiand_test_node and restores the file):

- P1 `probe_versioned_patch_of_a_vanished_id_conflicts`: 409, GET `n=new`, no `p`, version 1; the id only on the hash shard.
- P2 `probe_racing_writers_never_leave_two_copies`: 3 × 60 rounds, each re-seating the id off-hash and racing 2 PUT,
  1 DELETE, 1 PATCH and 1 UPSERT over HTTP. 0 rounds with two copies, 0 where GET or the SEARCH total disagree with
  the shards; ends on the hash shard 163 times, nowhere 17, never off-hash; statuses only 204 and 404 (no 0, 409 or 500).
  It is a timing probe: it does not report how often a write took the must_exist path, so it supports, and does
  not replace, the deterministic tests.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| N1 | no | gap | The replace_by_term and PATCH tests order the race with sleeps and pass vacuously if the erase runs before the lookup or read (the write then places on the hash shard anyway); nothing asserts the interleaving was reached. The fails-without jobs show it was reached in those runs. | tests/test_node.cc:1868, 1882-1895 | Assert the reached order (e.g. the lookup's turn is queued before the erase is started), or count it and report. |
| N2 | no | nit | `index(did)` catches `is_missing` (doc_not_found or not_found) around `replace_document`, whose `finish_write` commits after the write applied; a not_found from that commit would place the document a second time. Not reachable today (not_found is "No such database" on open). | src/database/handler.cc:474-479 | Catch `errc::doc_not_found` only. |
| N3 | no | nit | must_exist is ignored for a keyless document; every caller has a key, but nothing checks it. | src/engine/strata/shard.cc:866-869 | An assert that a must_exist write carries a key. |
| N4 | no | nit | A must_exist write that fails after its blob push leaves those blobs unreferenced in the found shard's storage (the journal says so). | src/database/shard.cc:742-770 | None needed now; the storage's sweep is where it belongs. |

## Not run

- No sanitizer build: the fix adds no shared state; the check runs inside the shard's existing turn.
- No crash or durability model (not this lens); WAL replay checked by reading only.
- An HTTP test of a versioned PUT (`?version=N`) on a vanished id: covered by reading (handler.cc index loop rethrows
  version_conflict when a version is given) and the engine test, not by a test of its own.
- No cluster (remote shards): strata has none (`require_local`).
