# exact-id-keys: journal

State: waiting on jobs 261113-142237-97068b 261113-142240-d936af 261113-142246-dbceae 261113-142257-1b699b 261113-142303-cc6119 261113-142310-ef98cf 261113-142316-4db9b3
Quest: .campaign/xapiand/quests/exact-id-keys.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Suite 261113-140055-efd7e7 at 83fea2fc4 aborted test_node in test_far_ids_keep_the_docids_dense (dense_s3) on
`assert(current_revision == prior_revision + 1)` in do_commit. Cause, pre-existing in the test: its merge_all called
`prospect::index::commit()` directly on a shard holding uncommitted far PUTs, a new revision the engine never noted
(`Core::note()` caches `committed_revision`); the shard's debounced autocommit then read the stale revision as prior
and saw +2. Fires only when the autocommit lands after the merge and before a refresh. Not this branch: the same
assert hit this test at one-id 8d61b1913 (job 261113-083755-2de9cb, an ancestor of master). Fixed at 7b23f319e:
merge_all commits through the shard, merges, syncs the merge output (strata bumps the revision only for applied
writes) and CHECKs engine revision == manifest revision. Also fixed the old-codec mutant, which no longer applied at
83fea2fc4 (job 261113-140102-b8b97c: MUTATION NOT APPLIED).

Jobs at 7b23f319e: suite 261113-142237-97068b (A4); A1 test 261113-142240-d936af, A1 mutant old-codec
261113-142246-dbceae; A2 test 261113-142257-1b699b, A2 mutant cap-2-32 261113-142303-cc6119; probe index-commit
mutant 261113-142310-ef98cf (exit 0 = the new CHECK fails without the fix). At master 07be4daa4: probe
261113-142316-4db9b3 (master's test plus only the revision CHECK; exit 0 = stale revision pre-existing).

## Next

1. When the jobs finish: paste brief's Evidence table, A3 row, State "ready for review" (or fix failures).

## Blocked on

Nothing. msg-46 to the lead: src/query_dsl.cc is outside the file set but the `_id` term search builds its term
there; edited minimally, see R5.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261113-142240-d936af (test); fails without: 261113-142246-dbceae (old-codec) |
| A2 | 261113-142257-1b699b (test); fails without: 261113-142303-cc6119 (cap-2-32) |
| A3 | [artifact] docs/design/exact-id-keys.md |
| A4 | suite 261113-142237-97068b; CI on quest/xapiand/exact-id-keys |

## Result

A numeric id's key is `N` + `sortable::encode(N)` (Kronuz/sortable ac13b77, pinned in CMakeLists.txt), exact for
every int64/uint64/double; 4294967297 and 4311744512 (and the other A1 pairs up to 2^64 - 1) are separate
documents. Code: src/serialise.{h,cc} (`Serialise::id`, `guess_id`, `Unserialise::id`), handler.{h,cc},
schema.cc, shard.cc, query_dsl.cc. Tests: tests/test_node.cc `test_ids_that_shared_a_key_are_separate_documents`
(plus key round-trip and byte order), `test_new_numeric_ids_pass_two_to_the_thirty_two`; mutants in
tests/exact_id_mutants.sh. Note: docs/design/exact-id-keys.md lists every key site.

A2: the auto-id cap is 2^63 - 1 (R2). A3: docs/design/one-id.md (outside the file set) still says 2^32 - 1.

## Rulings I made

- R1 One codec entry: `Serialise::id(spc, id)`, `Serialise::id(N)`, `Serialise::guess_id`, `Unserialise::id` in
  `src/serialise.cc`; sortable.hh is included there only. Every key site calls them (table in the design note).
- R2 `MAX_NUMERIC_ID` = 2^63 - 1, not 2^64 - 1: an issued id must fit every integer id type's stored `_id`
  (an integer field's is int64), and `high + ID_BUCKETS` cannot overflow below it. Positive ids above it keep
  exact keys and route by hash. A float placeholder refuses issued ids above 2^53 (`MAX_FLOAT_ID`, shard.cc).
- R3 `issue_id` keys every placeholder type with `numeric_key(issued)` (float and integer keys are one now); the
  id slot keeps the value codec (`Serialise::positive(issued)`, what index_item gives it).
- R4 test_document_ids_are_themselves: the integer list's 2^63 - 1 became 2^62, since 2^63 - 1 is now the cap and
  leaves no id for the POST that follows; the cap itself is tested in test_new_numeric_ids_pass_two_to_the_thirty_two.
  The old key-equals-value-codec checks in test_new_numeric_ids_keep_their_stride_across_a_reopen moved to the
  new tests with the new codec.
- R6 `unserialise_term_id` (response `_id` of POST, RESTORE, bulk) decodes 'N' keys with sortable in the id
  field's type: integer to int64, positive to uint64, floating to double.
- R5 src/query_dsl.cc edited outside the file set (msg-46): one branch in get_regular_query. docs/design/one-id.md
  (outside the file set) still says 2^32 - 1; the new note says it supersedes that bound.
- R7 test_far_ids' merge_all (tests/test_node.cc) commits through the shard, not the prospect index: the direct
  commit left the engine's cached revision behind and a later do_commit asserted. The fix is in the test, since no
  production path commits a revision behind the engine (commit_blocking and settle_merges commit only merge output,
  which strata gives no revision). `PoolTestAccess::engine` added for its CHECK; mutant `index-commit` restores it.
