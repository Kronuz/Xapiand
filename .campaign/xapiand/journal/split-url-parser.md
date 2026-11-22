# split-url-parser: journal

State: waiting on jobs 261121-145046-c7fc25
Quest: .campaign/xapiand/quests/split-url-parser.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Merged main into the branch (url-parser:657f7fe; only ci.yml changed, so earlier passes stand per the brief). Fixed-checker split job 261121-145046-c7fc25 at 657f7fe. Pushed; CI for A5 pending.

## Next

1. Read 261121-145046-c7fc25 (answer warnings, if any); cite CI at 657f7fe for A5; paste `campaign brief`'s Evidence table; answer split_check warnings (if any).

## Blocked on

Nothing.

## Evidence

Jobs (all at url-parser:e3dfd07):
- A1 build-test 261121-130327-f7cc1a
- A2 split 261121-130327-ed58df failed: the landed split_check.py treats docs/source-layout.md as
  C++ (8 errors, all on the note's lines; the known gap split-check-sources fixes). Rerun with that
  quest's tool per the lead's context: 261121-130734-9ce653. The lead reruns the gate check once it lands.
- A3 261121-130335-4deca1 failed: my command; unbuilt `ctest -N` prints "Could not find executable"
  lines naming each build dir, so the raw diff differs. Rerun filtered to `Test #` and `Total Tests`
  lines: 261121-130732-91be46.
- A6 261121-130338-635787 passed: no header added since 166bad2365da; selfcontained.sh -x url_parser.h
  finds no new header.

| Criterion | Evidence |
|---|---|
| A4 | `docs/source-layout.md` (url-parser:e3dfd07): each source file, and url_parser.cc kept whole with why |
| A7 | lead, 2026-11-21: read the split. url_parser.cc (1,400 lines) is kept whole and earns it: both parsers call one decoder engine in an anonymous namespace (a split would give ~700 hot lines external linkage), the byte loops are aligned in one TU, and five tests include the .cc. docs/source-layout.md says so. Agreed. |

## Result

No split. `url_parser.cc` (1,400 lines) stays whole; `docs/source-layout.md` (new) lists the
source files and gives the reasons. Nothing else changed.

## Rulings I made

1. url_parser.cc is kept whole (SPLITTING.md: "no split" is a valid result). Reasons, in the note:
   the decoder engine is an anonymous namespace both parsers call (urldecode2, Subst, find_byte3),
   so a parser in its own TU needs the ~700-line hot engine given external linkage in a detail
   header (a linkage change, not a move); URL_PARSER_ALIGN placement is speed-tuned per TU; five
   test sources `#include "../url_parser.cc"` and tools/mutants.py anchors all 28 mutants in it
   (its anchor check is a CTest test, and tools/mutants.py is outside this quest's file set).
2. Rejected alternative, recorded in the note: the decoder as a private one-includer header in an
   anonymous namespace (same TU). It adds a file and a second mutants source without making either
   half easier to work on.
3. Open-branch check: no branch of url-parser (pins-wave, origin/main) edits url_parser.cc.
