# catalog-restructure: journal

State: working
Quest: .campaign/xapiand/quests/catalog-restructure.md (the lead's; never edit it)

Code: strata worktree /Users/kronuz/code/Kronuz/strata-wt/xapiand/catalog-restructure, branch
quest/xapiand/catalog-restructure. Scratch: ~/.scratch/xapiand/catalog-restructure/ (the outline
map, build logs).

## Doing

Step 1 (member definitions out of the class) applied with cpp-libraries' tools/outline.py; local
build of the catalog test targets running before the commit.

## Next

1. Commit step 1 (the move alone), then docs; queue members, fnsizes and selfcontained at it.
2. Step 2: `catalog_detail::s3fifo` (catalog_s3fifo.hh) with its unit cases in
   test/catalog_policy_test.cc; mutants, one per invariant.

## Blocked on

Nothing yet (see "Step 1's nested types" once written).

## Step 1: the map

Class body on main (strata:cfa20b41): catalog.hh lines 259-2661, 2,403 lines. outline.py moved
135 multi-line member definitions (1,783 lines); 53 one-line definitions stay in the class.
After the move catalog.hh is 1,002 lines, the class body 734 (lines 259-993).

| Header | Mechanism | Definitions | Lines |
|---|---|---|---|
| catalog_table.hh | construction, teardown, chunks, chores and timers, host defaults | 16 | 189 |
| catalog_pin.hh | the pin protocol | 23 | 398 |
| catalog_transitions.hh | the slot transitions and shutdown, one state machine | 46 | 812 |
| catalog_policy.hh | the sweep and S3-FIFO | 22 | 291 |
| catalog_observe.hh | observation | 8 | 93 |

The map is ~/.scratch/xapiand/catalog-restructure/catalog.map, and is quoted in step 1's commit.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | |
| A2 | |
| A3 | |
| A4 | |
| A5 | |

## Result

<When ready for review.>

## Rulings I made

- R1 Headers beside catalog.hh as `catalog_<mechanism>.hh` (strata's include tree is flat), as
  index-restructure's R1.
- R2 One-line definitions stay in the class beside their declarations (53); every multi-line
  definition moves, nested types' included (pin_awaiter, shutdown_awaiter, pin's move and
  release, cancel_pin), as `catalog<Index, H>::pin_awaiter::f` definitions.
- R3 No `inline` on the out-of-class definitions: catalog is a class template (index-restructure
  R3, lead's 2026-11-21 ruling).
- R4 A fifth header, catalog_table.hh, holds what every mechanism shares (construction and
  teardown, the slots' chunks, chores and timers, the host's defaults, the call guard): none of the
  four mechanisms owns them. catalog_observe.hh is 93 moved lines (117 with its head), under
  SPLITTING.md's ~100-line floor; kept, as the quest names observation as a mechanism and its
  readers (tests, T4) are not the transitions'.
