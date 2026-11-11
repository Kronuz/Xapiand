# cross-shard-id: journal

State: waiting on jobs 261111-090543-e64abf 261111-090546-c325c5 261111-090549-756aed 261111-090551-a199d7 261111-090554-6173c1 261111-090558-1554c8 261111-090603-c42e4e
Quest: .campaign/xapiand/quests/cross-shard-id.md (the lead's; never edit it)

## Doing

Jobs queued at Xapiand:100856fca (head) and three mutants (local branches
`mutant/cross-shard-id/{no-must-exist,no-version-check,delete-joined-early}`, one diff each from the head).

## Next

1. Read the jobs; paste `campaign brief`'s Evidence table; CI on the branch (A4).

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | pending: 261111-090543-e64abf (head), 261111-090546-c325c5 (fails without: engine ignores must_exist) |
| A2 | pending: 261111-090543-e64abf (head asserts 204), 261111-090558-1554c8 (fails without: DELETE joined early) |
| A3 | pending: 261111-090543-e64abf, 261111-090549-756aed, 261111-090551-a199d7, 261111-090554-6173c1 |
| A4 | pending: suite 261111-090603-c42e4e; CI |

## Result

Brought over the prototype (Xapiand:22f84d654 probe, Xapiand:b7384c1f7 fix, cherry-picked from explore/cross-shard-id),
then Xapiand:100856fca:

- `Shard::replace` (src/engine/strata/shard.cc) with `must_exist`: when the key is not at the docid (erased, or at
  another docid) the write is first checked as one to no document (`check_version(expected, nullopt)`: an expected
  version answers version_conflict, as the pre-fix code did at that point) and then answers doc_not_found instead of
  creating. The prototype answered doc_not_found always, so `index(did)` placed a PATCH/UPDATE afresh with the version
  it read from the erased document; a new document of the id at that same version (a new one is at 1) was then
  overwritten with the old content plus the patch: a lost update the prototype introduced. Now the PATCH's
  read-modify-write loop retries and patches the new document (master behaved so too).
- Callers: `index(did)` and `replace_document_term` place afresh on doc_not_found (prototype, unchanged).
- A2, the DELETE's status 0: not a server defect. The test's DELETE has `?commit`; `Shard::finish_write` commits in a
  turn of its own, which queues behind the third hold, and the test joined the DELETE's thread before releasing that
  hold, so the client's 10 s `SO_RCVTIMEO` (tests/test_node.cc `http_call`) ended the call with no reply. The status
  was 0 before the fix too (261111-073927-6d9b0b). The test now checks the DELETE has not replied while the third hold
  is held, joins it after the release, and asserts 204; the mutant that joins early is the attribution job.
- Tests (tests/test_node.cc), sharing an `OffHashId` fixture: the engine answer per case
  (`test_a_replace_that_must_exist_writes_only_where_the_key_is`), the held-turn HTTP test (A1/A2), the
  `replace_document_term` caller (`..._replace_by_term_of_an_id_erased_since_its_lookup_places_it_afresh`), and the
  PATCH path through `index(did)` (`..._patch_of_an_id_erased_since_its_read_patches_the_new_document`).

The note docs/dev/cross-shard-id.md is outside this quest's file set; its "Results" should gain the A2 cause and the
version check (lead's call).

## Rulings I made

- must_exist checks the version as against no document before answering doc_not_found (above), keeping the engine
  the one place that decides; no handler-side version parsing.
- must_exist is checked for keyed documents only: every caller passes one (`index(did)` asserts a term,
  `replace_document_term` has one); a keyless replace keeps its old behavior.
- A must_exist failure after the blob push leaves those blobs unreferenced in the found shard's storage (the
  prototype restores the document's inline data so the retry pushes them again where it lands); same as any other
  failed replace there.
- The A2 fix is in the test (join order), not the server: a client that waits holds no turn, and the server replied
  once the turn came.
