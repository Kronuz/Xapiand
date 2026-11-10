# write-path-fixes: journal

State: waiting on jobs 261110-045411-83e91d 261110-045423-57a7f5 261110-045424-bc00ed 261110-045430-4ed4e9
Quest: .campaign/xapiand/quests/write-path-fixes.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Rebased onto master (clean); head 60a5e3736: DocVersionConflictError deleted with its line in
test_engine_error.cc (msg-17: the lead added the two files to the set; nothing throws the class, and
the 409 of errc::version_conflict stays covered by that test's answer table). Note:
docs/mechanisms/document-ids-and-versions.md (9fec11038). Jobs at the head:

- 261110-045411-83e91d: `campaign verify` build-test (ctest, async_check, removed_options; A3, A5).
- 261110-045430-4ed4e9: rerun of 261109-202752-ca0659 (engine strata + node cases; A1, A2).
- 261110-045423-57a7f5: docs E2E, `harness/run_all.sh smoke e2e tls` (A3).
- 261110-045424-bc00ed: TSan (llvm@22, slot wpf-tsan): test_engine_strata once, the four node cases
  three times each, halts nothing, counts "WARNING: ThreadSanitizer" per log (A4).

## Next

1. Read the four jobs; fill A3-A5 rows with them and A1/A2 with 261110-045430-4ed4e9; set State to
   "ready for review". E2E exit 137 / "Killed: 9" is the port-8880 clash: rerun.
2. CI (A5's second half) runs when the lead pushes; nothing to push from here.

## Cause (A1)

The id 0 was "no id" in three places, all keyed on the serialised 0 ("\x80"):

1. `Schema::index` (schema.cc, the `!document_id` branch) wrote a new numeric id (a POST to an index
   whose ids are numbers) under the key `N\x80`, the key of id 0, and skipped `set_key` and the key
   term for it; an explicit id 0 got the same key and the same skips.
2. `ShardEndpoint::do_replace_term` (shard.cc) read a numeric key whose value is 0 as "number it": it
   called `eng.add`, which takes the next docid, and `finalise_write` rewrote the id to that docid. So
   `PUT /z/0` on an empty index stored document 1 with id 1, and `PUT /z/1` then replaced it (version 2).
3. `if (!document_id)` / `document_id ? ...` (schema.cc, handler.cc) test MsgPack truthiness, so a body
   id `0` (RESTORE) was also "no id".

Underneath, numeric ids doubled as docids: `do_replace_term` replaced docid = id, so -1, 1.5, 2^63
became an out-of-range double-to-uint32 conversion, and a new id whose docid held another document
(a keyword or id-0 document placed there) replaced that document.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | Fixed: 261109-202752-ca0659 (1a72d3a7e, test_node all ok incl. test_document_ids_are_themselves; test_engine_strata 9768 ok). Fails at the base: 261109-202740-e5400a (555ceb738 = base b4acd8419 + only test_node.cc, same blob as head): the id test aborts on its first write, `PUT /ids_int/-1`, at `assert(did != 0)` in wal.cc:1101 (write_replace_document): id -1 converted to docid 0, the "ids double as docids" defect in Cause; setup is fine (the other three cases run on the same node). Asserts-off base run (no abort, every id checked): 261110-022045-f81f3a, 48 of 176 checks fail (GET/SEARCH/DELETE of the ids, test_node.cc:1217-1277). Mutants killed (each job exits 0 only when a named test fails): a 261110-022018-630bfa (19 of 176), e 261110-022023-91414c (engine placement :1224-1230 and node :1267), f 261110-022025-8d9d38 (engine :1243-1246), g 261110-022026-0969a8 (:1276-1277). At the head: 261110-045430-4ed4e9 (pending) |
| A2 | Fixed: 261109-202752-ca0659 (precondition, apply-once and model cases ok; model "48 won, 48 refused"). Fails at the base: 261109-202740-e5400a: precondition 33/111 checks fail (409 expected, `_version` 2/0 in the body, document kept at v2: test_node.cc:1300-1318), apply-once 16/38 (:1363 conflict `_version`, :1367 conflicts == writers-1), model 37/68 ("48 writes won, 0 refused"). Mutants killed: b 261110-022019-13681d (15 of 111), c 261110-022021-d4da86 (apply-once wins == 1, model version == seen + 1), d 261110-022022-9e2089 (missing doc 409, engine :1259), h 261110-022028-c214c0 (`_version` in the 409 body), i 261110-022029-baabc5 (missing doc with a version: 409). At the head: 261110-045430-4ed4e9 (pending) |
| A3 | build-test 261110-045411-83e91d and docs E2E 261110-045423-57a7f5 at 60a5e3736 (pending) |
| A4 | TSan 261110-045424-bc00ed at 60a5e3736 (pending) |
| A5 | build-test 261110-045411-83e91d (pending); CI when the lead pushes |

Jobs examined: 261109-183815-526fac (the explorer's probe at master 61ddc922d: Q3 and the id-0 side
check; its "control PUT ?version=1" ran after a DELETE and a PUT recreated the document at version 1,
so that 204 was right: PUT on an existing document already checked the version). 261109-201412-8dd2a3
and 261109-202042-42e7bb cancelled before running (superseded).

## Result

<When ready for review.>

## Rulings I made

1. A new numeric id has its own key, `NEW_NUMERIC_KEY` = "N" (handler.h): a numeric key always has a
   value after its 'N', so no explicit id equals it. Every explicit id, numeric ones included, is
   written by its key (`replace_by_key`). A new numeric id that is a whole number in [1, 2^32-1]
   (`numeric_docid`) asks for the docid it names (`WriteArgs::new_did`) and takes it when free, else
   the engine numbers it after its last docid. This keeps the ids the shard numbers (last docid + 1)
   free of explicit ids: an explicit id k lands at docid k unless docid k was taken when k was first
   written, and then k is at most the last docid. Numeric ids outside that range (0, negatives,
   fractions, beyond 2^32) are placed by their key (multi-shard: by the key's hash, like keywords).
   Existing indexes keep their layout: in-range ids already sat at docid = id.
2. A replace by docid of a document whose key is not found, at a docid that holds another document,
   is refused with a version conflict reporting 0 (the keyed document does not exist) instead of
   overwriting it; an unversioned write retries (CONFLICT_RETRIES) and is placed by its key. WAL
   replay (replayed documents) keeps its old behaviour.
3. `?version=N` is a precondition checked first, for PUT, PATCH, UPDATE (merge), UPSERT and DELETE
   alike: the document's version must be N, 0 meaning "does not exist". A missing document with N > 0
   is 409 for every method, DELETE included: Elasticsearch answers `if_seq_no` on a missing document
   with 409 (version_conflict_engine_exception, "but no document was found") for index and delete.
   With N = 0 on a missing document the method does what it does without a version: PUT and UPSERT
   create, PATCH, UPDATE and DELETE are 404.
4. The 409 body carries the current version as `_version` (0: no document), from
   `engine::Error::current_version`, set by `engine::version_conflict(current)` in every place a
   version conflict is raised (strata's check_version and verdict, the handler's early check); its
   message says it too. No new request parameter.
5. `has_document_id` (handler.h): undefined, null and the empty string are no id; any number, 0
   included, is one. Replaces the MsgPack truthiness tests in schema.cc and handler.cc.
6. Numeric ids are serialised as doubles (sortable_serialise), so ids past 2^53 that round to one
   double share a key. Pre-existing and outside this quest; the largest integer ids round trip.
