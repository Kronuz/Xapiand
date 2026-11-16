# serialize-repin: journal

State: waiting on jobs 261116-051529-669e33 261116-051529-20615b 261116-051829-ed3fbe 261116-051832-47864e 261116-051859-74d69e
Quest: .campaign/xapiand/quests/serialize-repin.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Pins, rename and alias deletion committed and pushed. Jobs running: build-test 261116-051529-669e33,
e2e 261116-051529-20615b (at 0900fc8; async_check/test_gate_tools edit came after: rerun with `campaign verify`
if they pass), A2 grep 261116-051829-ed3fbe, A4 harness 261116-051832-47864e, A4 mutant 261116-051859-74d69e.

## Next

1. Wait on jobs; fix build errors from the renamed library APIs if any.

## Blocked on

A2: `oldtests/` (6 files, names and content) and `docs_to_postman.py:281` (a comment) still say serialis*;
neither is in my file set. Options: (a) add them to the set so I rename them (recommended; oldtests is a
legacy suite CMake only builds on demand), (b) exclude them from the A2 grep. The A2 job fails until then.
A4: `query-old-codec` likely survives (ids and values share one codec, per sortable-encoding's journal);
see the mutant job's result.

## Evidence

See `campaign brief serialize-repin`.

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
