# exact-id-keys: journal

State: working
Quest: .campaign/xapiand/quests/exact-id-keys.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Implementation and tests committed; compile check job 261113-132249-f20e66 running. Next: run the node tests,
then the A1/A2 mutants and `campaign verify --for A4`.

## Next

1. Compile check passes (fix errors), then submit `xapiand_test_node` cases for A1/A2 and the three mutants.
2. `campaign verify exact-id-keys --for A4`; push the branch for CI's sanitizers.
3. Paste brief's Evidence table, write Result, State "ready for review".

## Blocked on

Nothing. msg-46 to the lead: src/query_dsl.cc is outside the file set but the `_id` term search builds its term
there; edited minimally (default stated in the message), see Rulings.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | pending |
| A2 | pending |
| A3 | `docs/design/exact-id-keys.md` |
| A4 | pending |

## Result

<pending>

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
- R5 src/query_dsl.cc edited outside the file set (msg-46): one branch in get_regular_query. docs/design/one-id.md
  (outside the file set) still says 2^32 - 1; the new note says it supersedes that bound.
