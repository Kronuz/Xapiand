# split-htm: journal

State: waiting on jobs 261121-131843-3e7231 261121-131843-0b551d 261121-131843-5adddc 261121-132934-57906c
Quest: .campaign/xapiand/quests/split-htm.md (the lead's; never edit it)

## Doing

Lead's A7 ruling applied: split reverted (htm:a3dc52a, a `git revert` of htm:87bc3b8), layout note
rewritten to name htm.cc as kept whole (htm:f20c061, pushed). A1/A3 rerun at htm:f20c061.

## Next

1. When the jobs finish: paste `campaign brief`'s table below, set State "ready for review".

## Blocked on

Nothing. The lead's ruling cleared both earlier blockers: AGENTS.md, ARCHITECTURE.md, README.md
and test/gen_golden.cc stay accurate with htm.cc whole, and Xapiand needs no change. The lead
runs the main merge (ci.yml) at landing.

## Messages

- Lead's A7 ruling (keep htm.cc whole, revert, rewrite the note, rerun A1 and A3): done as asked;
  revert htm:a3dc52a, note htm:f20c061, jobs 261121-131843-3e7231 (build-test, A1),
  261121-131843-5adddc (ctest -N diff, A3), split gate 261121-131843-0b551d.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | build-test 261121-131843-3e7231 at htm:f20c061 (pending) |
| A2 | split gate 261121-131843-0b551d at htm:f20c061 (pending). The tree now differs from main only in `docs/source-layout.md`: no C/C++ line moved. Earlier 261121-130530-dc9d96 (at htm:8e7cad4, the split) failed with the landed tool on non-C/C++ files and include comments; superseded. |
| A3 | 261121-131843-5adddc at htm:f20c061 (pending): configures head and base (htm:0c09a02), diffs `ctest -N` |
| A4 | `docs/source-layout.md` at htm:f20c061: htm.cc named kept whole, with the reasons and the cut to make |
| A5 | CI on the pushed branch (CI's) |
| A6 | 261121-132934-57906c at htm:f20c061 (pending; rerun of 261121-130535-f05536, which passed at htm:8e7cad4): the quest creates no header |
| A7 | Lead's read: keep htm.cc whole (message above) |

Superseded jobs: 261121-130530-a1b9d9 (build-test at htm:8e7cad4, cancelled),
261121-130532-356ce9 (ctest -N at htm:8e7cad4, cancelled by its replacement).

## Result

htm.cc (1,051 lines) stays whole, by the lead's A7 ruling. `docs/source-layout.md` names each
source and test file, lists htm.cc as kept whole with why (consumers compile it by path, so a
split breaks every consumer's link until each repins; 5% over the mark with clear sections), and
records the cut to make if the owner ever wants one: `htm_set_ops.cc` (old lines 34-382, 490-559),
`htm.cc` (385-487, 851-1051), `htm_descent.cc` (562-848), plus the `HTM_SOURCES` list, the four
docs, and the consumer pin-bump step. The tried split (htm:87bc3b8) is in the branch history,
reverted. Net change against main: `docs/source-layout.md` only.

## Rulings I made

1. (Superseded by the lead's A7 ruling.) I first split htm.cc into three files at the cut above.
2. No open branch edits htm.cc: explore/htm-edge-nearest, pins-wave and
   origin/quest/xapiand/htm-edge-points are all merged into main.
3. The revert's message was amended before the push to say why (local, never pushed before).
4. A3 is one job that configures both commits and diffs (`--ref BASE=`), rather than two jobs.
