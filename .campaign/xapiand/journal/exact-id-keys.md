# exact-id-keys: journal

State: waiting on jobs 261115-210614-d6a201 261115-210629-01840c 261115-210632-ca9170 261115-210635-274f76 261115-210639-ed1c27
Quest: .campaign/xapiand/quests/exact-id-keys.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Master merged into the branch (68f26a25e, no conflicts). Gate at the merged head: suite 261115-210614-d6a201 (A4);
A1 test 261115-210629-01840c, A1 mutant old-codec 261115-210632-ca9170; A2 test 261115-210635-274f76, A2 mutant
cap-2-32 261115-210639-ed1c27. docs/design/exact-id-keys.md (A3) was already written and committed at c4d10938d;
checked against the final code, no em dashes, no GPL source read.

## Next

1. When the jobs finish: if all pass, State "ready for review" and update the Evidence table with these ids;
   if one fails, diagnose it (merge interaction first).

## Blocked on

Nothing. msg-46 to the lead: src/query_dsl.cc is outside the file set but the `_id` term search builds its term
there; edited minimally, see R5.

## Evidence

At the merged head 68f26a25e (earlier runs at 7b23f319e: A1 261113-142240-d936af / 261113-142246-dbceae, A2
261113-142257-1b699b / 261113-142303-cc6119, suite 261113-142237-97068b, all exit 0).

| Criterion | Evidence |
|---|---|
| A1 | 261115-210629-01840c (test); fails without: 261115-210632-ca9170 (old-codec) |
| A2 | 261115-210635-274f76 (test); fails without: 261115-210639-ed1c27 (cap-2-32) |
| A3 | [artifact] docs/design/exact-id-keys.md |
| A4 | suite 261115-210614-d6a201; CI on quest/xapiand/exact-id-keys |

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
