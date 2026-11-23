# split-uinteger-t: journal

State: waiting on job 261121-145044-02dfce (A2, split_check from split-check-sources, at 8a6fe9030aab, after merging origin/master)
Quest: .campaign/xapiand/quests/split-uinteger-t.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Lead ruling applied at 6815126 (branch pushed): uinteger_t.hh is whole again, byte-identical to master; only test/property.cc is split (8 pieces). Waiting on the jobs above. A3: diff `campaign job log 261121-115203-b998b3` (before, f9dd8e4) against `261121-121040-919d81` (after, 6815126).

## Next

1. Read the jobs; record A1/A3/A6; answer the split check's warnings once split-check lands (expected: none, only includes, `#pragma once` and comments are added).
2. A7: the lead reads the split (docs/source-layout.md).

## Blocked on

Nothing. AGENTS.md's repo map line for `test/property.cc` could gain "its checks live in test/property/*.hh" (outside my file set, the lead's call); tools/mutate.py, tools/mutants/*.txt and bench/*.sh are not stale now: uinteger_t.hh is unchanged, and mutate.py copies test/ recursively.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | build-test job 261121-121039-c6d5e2 at 6815126e3170 |
| A2 | 261121-145044-02dfce (fixed split_check, 8a6fe9030aab; replaces 261121-123004-c2422b, which read .md as code); the size limit is gone |
| A3 | ctest -N, sorted and unnumbered: before 261121-115203-b998b3 (f9dd8e4c03c7), after 261121-121040-919d81 (6815126e3170) |
| A4 | [artifact] docs/source-layout.md: the 8 test pieces, and uinteger_t.hh kept whole with why |
| A5 | CI on the pushed branch |
| A6 | job 261121-121044-660a41 at 6815126e3170: the eight test/property/*.hh each included twice, `c++ -std=c++17 -fsyntax-only -I. -Itest` (tools/selfcontained.sh skips test/ by default, so the loop is the job command); earlier job 261121-115721-e5c431 passed at 5cad82f |
| A7 | lead, 2026-11-21: read the split; property.cc split by topic into self-contained headers kept in one TU (static linkage kept, two CTest targets build property.cc; reason in docs/source-layout.md); accepted |

## Result

test/property.cc 1,595 -> 110 lines plus 8 pieces in test/property/ (131 to 258 lines, none under 100), cut by topic: shared helpers, multiplication kernels, digit references, radix kernels, decoder and guards, division shapes, operators, representation. uinteger_t.hh is untouched.

## Rulings I made

- config.hh (77 lines) and free_operators.hh (160 lines) went back into uinteger_t.hh: the first is under the 100-line floor, the second is not something a maintainer opens alone, and the library's contract is one file. The 16 `.inc` pieces went back too (the lead's ruling); the branch's history holds the split and its reversal, and the final tree has no trace of them.
- Test pieces are `.hh` of the one translation unit test/property.cc, so every `uinteger_t_property*` CTest target is untouched and CMakeLists needs no edit. They use `#pragma once` (one added line each). common.hh took the standard includes, the private-access define, the PROPERTY_DEFAULT_CUTOFFS block and `#include "uinteger_t.hh"` from the top of property.cc, so every piece compiles alone. `rng`, the only namespace-scope object with a dynamic initializer, stays first in common.hh. No helper was added or made shared: no linkage change.
