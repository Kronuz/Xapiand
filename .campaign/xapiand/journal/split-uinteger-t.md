# split-uinteger-t: journal

State: waiting on jobs 261121-115150-2b1908 (build-test), 261121-115150-c59ac9 (split), 261121-115157-404e92 (ctest -N after), 261121-115203-b998b3 (ctest -N before); split-check has not landed, so the split job may not pass until it does
Quest: .campaign/xapiand/quests/split-uinteger-t.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Code is done and pushed on quest/xapiand/split-uinteger-t (uinteger_t-wt worktree): three commits, one per split file plus the docs.
Waiting on the jobs above. Diff the two ctest -N logs (`campaign job log 261121-115203-b998b3` vs `261121-115157-404e92`) for A3.

## Next

1. Read the jobs; record A1/A3 results. If the split check is red or absent, answer its warnings here once split-check lands.
2. Hand off to the lead: bench/ab.sh, bench/run.sh and tools/mutants/*.txt (outside my file set) assume the library is the single file uinteger_t.hh (see Blocked on).

## Blocked on

Not blocking this quest, for the lead: tools/mutate.py edits only `uinteger_t.hh` text and copies top-level `*.hh` files, so its `find:` strings now match code that lives in `uinteger_t/*.hh` and the pieces are not copied into its temporary tree; the mutation suites (tools/mutants/*.txt, `header: uinteger_t.hh`) need to name the pieces (or the tool expand includes). bench/ab.sh, bench/run.sh and bench/common.sh copy or checksum only `uinteger_t.hh` (`git show REF:uinteger_t.hh`), so an A/B of this commit against an older ref needs the `uinteger_t/` directory copied too. AGENTS.md's repo map still says "uinteger_t.hh — the entire library". All outside my file set (tools/**, bench/**, AGENTS.md).

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | build-test job 261121-115150-2b1908 at cd73b7e81cc3 |
| A2 | split job 261121-115150-c59ac9; split-check had not landed when the code was done |
| A3 | ctest -N jobs: before 261121-115203-b998b3 (f9dd8e4c03c7), after 261121-115157-404e92 (cd73b7e81cc3); diff of their sorted outputs |
| A4 | [artifact] docs/source-layout.md (commit cd73b7e) lists every piece and what it holds |
| A5 | CI on the pushed branch |

Mechanical proof that code moved unchanged (scratch script, not a job): expanding each `#include "uinteger_t/..."` and `"property/..."` back in place and dropping blank lines gives exactly the original non-blank lines of uinteger_t.hh (5495) and test/property.cc (1497), no differences.

## Result

uinteger_t.hh 6,042 -> 129 lines plus 18 pieces in uinteger_t/ (77 to 519 lines); test/property.cc 1,595 -> 110 lines plus 8 pieces in test/property/ (126 to 252 lines). The class body pieces are included inside the class at the point the code was, in the original order; no member moved. Layout in docs/source-layout.md.

## Rulings I made

- The class is one class, so its pieces are fragments included inside `class uinteger_t { ... }`, not standalone headers (docs/source-layout.md says so). Out-of-class definitions would have changed code.
- No header guards on fragments: each is included once, from uinteger_t.hh. The fragments need no `#pragma once`.
- Compiler/platform macros (lines 58-133 of the old header) moved to uinteger_t/config.hh and the free operators plus std overloads to uinteger_t/free_operators.hh; `#undef UINTEGER_T_PRIVATE` stays in uinteger_t.hh, where the macro is defined.
- Test pieces are `.hh` fragments of the one translation unit test/property.cc, so all the `uinteger_t_property*` CTest targets (same source file, different defines) are untouched and CMakeLists needs no edit. The only namespace-scope object with a dynamic initializer, `rng`, stays first, in test/property/common.hh, before everything that uses it. No shared helper was added or moved; no CMake install rules exist in this repo (INTERFACE library on the source directory), so none changed.
- Each piece's first line is a one-line comment (the only added lines besides the `#include`s); blank lines at piece edges were dropped.
