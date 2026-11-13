# exact-id-keys: journal

State: waiting on jobs 261113-140055-efd7e7 261113-140057-32e4ef 261113-140102-b8b97c 261113-140111-a97edc
Quest: .campaign/xapiand/quests/exact-id-keys.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Suite 261113-133034-97a8fa failed at 58a14faf6: every POST echoed a wrong `_id` because
`DatabaseHandler::unserialise_term_id` decoded 'N' keys with the value codec. Fixed at 83fea2fc4
(`Unserialise::id(FieldType, bytes)`). Jobs at 83fea2fc4: suite 261113-140055-efd7e7 (A4), A1 test
261113-140057-32e4ef, A1 mutant old-codec 261113-140102-b8b97c (--fails-without), follow-up cap-2-32 mutant probe
261113-140111-a97edc (exit 0 = killed; no criterion since A2 was dropped). Branch pushed for CI.

## Next

1. When the jobs finish: paste brief's Evidence table, State "ready for review" (or fix failures).

## Blocked on

Nothing. msg-46 to the lead: src/query_dsl.cc is outside the file set but the `_id` term search builds its term
there; edited minimally, see R5.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | jobs 261113-140057-32e4ef (test), 261113-140102-b8b97c (old-codec mutant) |
| A4 | suite 261113-140055-efd7e7; CI on quest/xapiand/exact-id-keys |

## Result

A numeric id's key is `N` + `sortable::encode(N)` (Kronuz/sortable ac13b77, pinned in CMakeLists.txt), exact for
every int64/uint64/double; 4294967297 and 4311744512 (and the other A1 pairs up to 2^64 - 1) are separate
documents. Code: src/serialise.{h,cc} (`Serialise::id`, `guess_id`, `Unserialise::id`), handler.{h,cc},
schema.cc, shard.cc, query_dsl.cc. Tests: tests/test_node.cc `test_ids_that_shared_a_key_are_separate_documents`
(plus key round-trip and byte order), `test_new_numeric_ids_pass_two_to_the_thirty_two`; mutants in
tests/exact_id_mutants.sh. Note: docs/design/exact-id-keys.md lists every key site.

Follow-ups (dropped A2/A3, already built on this branch but not proven by jobs): the auto-id cap raise to
2^63 - 1 (mutant `cap-2-32` exists, not run) and the design note; docs/design/one-id.md (outside the file set)
still says 2^32 - 1.

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
