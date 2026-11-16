# serialize-repin: journal

State: waiting on jobs 261116-052345-3145cd 261116-052345-b9b1f8 261116-052347-b2423b 261116-052351-094a46 261116-052354-4dcdf7
Quest: .campaign/xapiand/quests/serialize-repin.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Lead rulings applied (xapiand:ef9b4b4cf): oldtests/** and docs_to_postman.py renamed; mutant query-old-codec replaced
by query-double (numeric _id term encoded via MsgPack(obj.as_f64()); the existing test
test_ids_that_shared_a_key_are_separate_documents already looks up 2^53-1 and 2^53+1). Jobs at head: build-test
261116-052345-3145cd, e2e 261116-052345-b9b1f8, A2 grep 261116-052347-b2423b, A4 harness 261116-052351-094a46,
A4 mutant (--fails-without) 261116-052354-4dcdf7.

## Next

1. On finish: if the mutant job fails (survives or does not apply), fix the mutation text; then State ready for review.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | build-test 261116-064759-26b223 at 5b8e4589b; CI 43889549864, FreeBSD 43889549933 |
| A2 | grep 261116-064804-6644f0 |
| A3 | e2e 261116-064759-a1e3d1 |
| A4 | harness 261116-064804-511a73; mutant query-double 261116-064804-835cd1 |

## Result

Rename done mechanically (307 lines in 75 files). Exceptions kept (Xapian proper names): `sortable_serialise` /
`sortable_unserialise` (docs/** provenance notes only), `sortable-serialise` (docs/design/extraction.md),
`serialise-double` / `serialise-double.h` (docs/status/engine-search.md), `Xapian::SerialisationError`
(docs/reviews, docs/status), `Xapian::Document::serialise` / `unserialise` (docs/reviews/engine-core.md), and
Xapian's global `::serialise_double` (docs/status/x1-tests.md:58).

## Rulings I made

- varint pinned in CMakeLists.txt (new Declare before strata's, so strata fetches that one); flume is not
  fetched by Xapiand, strata or prospect, so it has no pin to move.
- Removed `r"^src/sortable_serialise\."` from harness/async_check.py NEVER_OPEN and its fixture line in
  harness/test_gate_tools.py: the files are deleted. test_gate_tools passes (21 tests).
- The self-alias `using SerializationError = SerializationError` the rename left is deleted with the old alias.
