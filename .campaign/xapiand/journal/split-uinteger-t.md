# split-uinteger-t: journal

State: waiting on jobs 261121-115717-15c031 (build-test), 261121-115721-e5c431 (A6), 261121-115203-b998b3 (ctest -N before); blocked on the lead for the class-body pieces (see Blocked on); the split check cannot run until split-check lands
Quest: .campaign/xapiand/quests/split-uinteger-t.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Code is done at 5cad82f (branch quest/xapiand/split-uinteger-t). Waiting on the jobs above. A3: diff `campaign job log 261121-115203-b998b3` (before, f9dd8e4) against `261121-115718-f6f3c7` (after, 5cad82f).

## Next

1. Read the jobs; record A1/A3/A6. Answer the split check's warnings once split-check lands.
2. The lead rules on the class-body pieces (below).

## Blocked on

1. A6 vs "code moves unchanged" for `uinteger_t.hh` (the lead decides). The 16 pieces of the class body are member declarations inside one `class uinteger_t { ... }`; a member function cannot compile outside its class, so they cannot be self-contained headers unless every member becomes an out-of-class definition (every signature rewritten with `uinteger_t::`, templates and static constexpr data redeclared: the code changes, which the quest forbids, over ~5,500 lines, high risk). What I did: they are `uinteger_t/*.inc` (class-body fragments, not headers, so A6 does not apply to them; docs/source-layout.md says so). The real headers are self-contained: `uinteger_t/config.hh`, `uinteger_t/free_operators.hh` and the eight `test/property/*.hh` pieces (A6 job). Options: (a) keep `.inc` fragments (recommended: moves only, tests and users unchanged); (b) out-of-class definitions, as a separate quest with its own review; (c) fewer, larger fragments. I stopped there.
2. Outside my file set, for the lead: tools/mutate.py edits only `uinteger_t.hh` text and copies top-level `*.hh` files, so its `find:` strings now match code in `uinteger_t/*.inc` and the pieces are not copied into its temporary tree (tools/mutants/*.txt say `header: uinteger_t.hh`); bench/ab.sh, bench/run.sh and bench/common.sh copy or checksum only `uinteger_t.hh` (`git show REF:uinteger_t.hh`), so an A/B against an older ref needs `uinteger_t/` copied too; AGENTS.md's repo map still says "uinteger_t.hh - the entire library".

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | build-test job 261121-115717-15c031 at 5cad82f19e4d |
| A2 | split job 261121-115717-fac0fc failed exit 2 (the tool is absent: split-check has not landed); to rerun after it lands |
| A3 | ctest -N, sorted and unnumbered: before 261121-115203-b998b3 (f9dd8e4c03c7), after 261121-115718-f6f3c7 (5cad82f19e4d); diff recorded below |
| A4 | [artifact] docs/source-layout.md (commit 5cad82f) lists every piece and what it holds |
| A5 | CI on the pushed branch |
| A6 | job 261121-115721-e5c431 at 5cad82f19e4d: config.hh, free_operators.hh and test/property/*.hh each included twice, `c++ -std=c++17 -fsyntax-only -I. -Itest`, the method of tools/selfcontained.sh (which skips test/ by default, so the loop is a job command); .inc fragments excluded, see Blocked on |

Mechanical proof that code moved unchanged (scratch script, not a job): expanding each `#include "uinteger_t/..."` and `"property/..."` back in place and dropping blank lines gives exactly the original non-blank lines of uinteger_t.hh (5495) and test/property.cc (1497), no differences.

## Result

uinteger_t.hh 6,042 -> 129 lines plus 16 `.inc` class-body pieces and 2 headers in uinteger_t/ (77 to 519 lines); test/property.cc 1,595 -> 110 lines plus 8 pieces in test/property/ (126 to 252 lines). The `.inc` pieces are included inside the class at the point the code was, in the original order; no member moved. Layout in docs/source-layout.md.

## Rulings I made

- The class is one class, so its pieces are `.inc` fragments included inside `class uinteger_t { ... }`, not headers (see Blocked on 1). No guards on them: each is included once.
- New real headers (config.hh, free_operators.hh, test/property/*.hh) use `#pragma once`, one added line each, rather than a three-line `#ifndef` guard (uinteger_t.hh keeps its own). free_operators.hh includes `../uinteger_t.hh`, which includes it back after the class; the guard breaks the cycle either way round. common.hh took the standard includes, the private-access define and uinteger_t.hh from the top of property.cc so the pieces compile alone.
- Compiler/platform macros (lines 58-133 of the old header) moved to uinteger_t/config.hh and the free operators plus std overloads to uinteger_t/free_operators.hh; `#undef UINTEGER_T_PRIVATE` stays in uinteger_t.hh, where the macro is defined.
- Test pieces are `.hh` fragments of the one translation unit test/property.cc, so all the `uinteger_t_property*` CTest targets (same source file, different defines) are untouched and CMakeLists needs no edit. The only namespace-scope object with a dynamic initializer, `rng`, stays first, in test/property/common.hh, before everything that uses it. No shared helper was added or moved; no CMake install rules exist in this repo (INTERFACE library on the source directory), so none changed.
- Each piece's first line is a one-line comment (the only added lines besides the `#include`s); blank lines at piece edges were dropped.
