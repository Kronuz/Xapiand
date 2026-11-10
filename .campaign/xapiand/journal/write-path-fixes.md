# write-path-fixes: journal

State: waiting on jobs 261109-202752-ca0659 261109-202740-e5400a
Quest: .campaign/xapiand/quests/write-path-fixes.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Code, tests and note committed; head 32fe790e8 (code as built by 261109-202752-ca0659 plus a one-line
std::string conversion in numeric_docid). Running (msg-11..13 closed: the file set has test_node.cc,
pool.h, the note and exception_engine.h):

- Mutants a-i (slot wpf-mut, refs/scratch/wpf-mutant-X off 32fe790e8; a job exits 0 when a named test
  fails, i.e. killed): 261110-022018-630bfa (a), 022019-13681d (b), 022021-d4da86 (c),
  022022-9e2089 (d), 022023-91414c (e), 022025-8d9d38 (f), 022026-0969a8 (g), 022028-c214c0 (h),
  022029-baabc5 (i).
- 261110-022045-f81f3a: the id test at the base with asserts off, to show the id checks it fails
  (the asserts build aborts on its first write, see Evidence).

## Next

1. Read the mutant jobs; a survivor means a missing test.
2. DocVersionConflictError: deleting it needs tests/test_engine_error.cc:151 (its other use), outside
   the set; asked the lead (msg-17). Settle the head on the answer.
3. At the head marked ready, once each: TSan job (llvm@22 recipe, build-tsan, the four node cases and
   test_engine_strata; A4), `campaign verify write-path-fixes` (build-test; A3, A5) and the docs E2E
   job (`harness/run_all.sh smoke e2e tls`; A3). E2E exit 137 / "Killed: 9" is the port-8880 clash:
   rerun.

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
| A1 | pending |
| A2 | pending |
| A3 | pending |
| A4 | pending |
| A5 | pending |

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
