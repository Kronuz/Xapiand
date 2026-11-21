# split-htm: journal

State: waiting on jobs 261121-130530-a1b9d9 261121-130530-dc9d96 261121-130532-356ce9 261121-130535-f05536
Quest: .campaign/xapiand/quests/split-htm.md (the lead's; never edit it)

## Doing

Split done at htm:8e7cad4 (pushed); gate and evidence jobs queued.

## Next

1. When the jobs finish: paste `campaign brief`'s table below, set State "ready for review" (or
   fix what failed).

## Blocked on

The lead, outside this quest's file set (not blocking the split itself; docs go stale on landing):

1. Docs naming `htm.cc` as the whole implementation: `AGENTS.md` (repo map line 12, lines 27, 127,
   130), `ARCHITECTURE.md` ("Two headers and two compiled translation units", lines 15, 70), and
   the build command in `test/gen_golden.cc` line 4 (needs `htm_descent.cc htm_set_ops.cc` added,
   or it fails to link). Options: (a) widen the file set to `AGENTS.md`, `ARCHITECTURE.md`,
   `README.md`, `test/gen_golden.cc` and I update them in this quest; (b) a follow-up quest.
   Recommend (a): four small edits, and the split should not land with a broken regen command.
2. Xapiand compiles htm sources by path (Xapiand `CMakeLists.txt` line 1214 lists
   `${htm_SOURCE_DIR}/htm.cc`). When its htm pin moves past this split, that list must add
   `htm_descent.cc` and `htm_set_ops.cc`, or Xapiand fails to link. Same pattern as split-url-parser.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | build-test 261121-130530-a1b9d9 (pending) |
| A2 | split 261121-130530-dc9d96 (pending); local run of the split-check-sources tool: 0 errors, 3 warnings, answered below |
| A3 | 261121-130532-356ce9 (pending): configures head and base (htm:0c09a02), diffs `ctest -N` |
| A4 | `docs/source-layout.md` at htm:8e7cad4 |
| A5 | CI on the pushed branch (CI's) |
| A6 | 261121-130535-f05536 (pending): the quest creates no header, so no header to check |
| A7 | lead's read |

split_check warnings (all three are htm.cc include lines whose trailing comments changed):

1. `<algorithm> // for std::merge, std::back_inserter`: merge moved to htm_set_ops.cc (its own
   `<algorithm>` line); htm.cc still uses `std::clamp`, comment now says so.
2. `<cmath> // ... std::cos`: htm.cc keeps the include; comment drops `std::cos` (unused anywhere)
   and the descent's `std::sqrt` moved with its own `<cmath>` line.
3. `<iterator> // for std::make_move_iterator`: every use moved to htm_set_ops.cc, which includes
   `<iterator>`; htm.cc no longer needs it.

## Result

htm.cc (1,051 lines) split into three units, moves only (htm:87bc3b8), layout note (htm:8e7cad4):

1. `htm_descent.cc` (317): `getId(Cartesian)`, the fusion-safe descent with its macros, MSVC
   `fp_contract(off)` region, probes and form choice. Its floating-point rules apply only here.
2. `htm_set_ops.cc` (452): trixel/range union, intersection, exclusive disjunction and their
   static helpers, then simplifyTrixels/simplifyRanges (ARCHITECTURE.md groups them as set ops).
3. `htm.cc` (340): geometry helpers, names, ids from names, ranges, getTrixels, getCorners.

`CMakeLists.txt`: one `HTM_SOURCES` list feeds `htm`, `htm_fma` and `htm_rounded`.
Docs: `docs/source-layout.md`. No file in the repo is over 1,000 lines now.

## Rulings I made

1. Split rather than keep whole: the descent is a self-contained unit with its own floating-point
   rules (and an MSVC pragma that applies to everything between its on/off lines); the set ops are
   a separate algorithm family; neither calls the other. Each piece is over 300 lines.
2. Simplify goes with the set ops as a second slice (old lines 490-559) rather than staying with
   the geometry, matching ARCHITECTURE.md's "Set operations" section.
3. No open branch edits htm.cc: explore/htm-edge-nearest, pins-wave and
   origin/quest/xapiand/htm-edge-points are all merged into main.
4. Each piece's first line is a comment saying what it holds, above the license (datetime split
   precedent); htm.cc gets one too.
5. `HTM_SOURCES` variable instead of three copies of the source list (CMake is in the file set).
6. A3 is one job that configures both commits and diffs (`--ref BASE=`), rather than two jobs.
