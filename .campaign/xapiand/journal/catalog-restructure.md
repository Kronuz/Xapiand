# catalog-restructure: journal

State: working
Quest: .campaign/xapiand/quests/catalog-restructure.md (the lead's; never edit it)

Code: strata worktree /Users/kronuz/code/Kronuz/strata-wt/xapiand/catalog-restructure, branch
quest/xapiand/catalog-restructure. Scratch: ~/.scratch/xapiand/catalog-restructure/ (the outline
map, build logs).

## Doing

Step 2 built locally (Debug, the 12 catalog targets); its local test run is in a light hold.
Then commit step 2, make the mutant commits (~/.scratch/xapiand/catalog-restructure/mutants.py)
and queue the evidence at the ready head.

## Next

1. Commit step 2; mutants (9, one per policy invariant), build-test (`campaign verify`), TSan on the
   catalog tests, selfcontained at the head; push.

## Blocked on

The lead: step 1's nested types. The quest's step 1 also asks that slot, host, pin, pin_awaiter
and shutdown_awaiter be defined outside the class body (`class catalog::pin { ... }`), proved by
the `members` check. split_check.py --members cannot prove that (it parses no out-of-class nested
class definition; outline.py moves only member functions), and cpp-libraries' SPLITTING.md says
the opposite: "The class keeps its data members, nested types and one declaration per member
function". Step 1 as built keeps the nested types in the class and moves their multi-line members
(pin_awaiter's, shutdown_awaiter's, pin's move and release) out. Options:
1. Keep them in the class (SPLITTING.md's rule). The body is 727 lines, ~200 of them nested
   types. Recommended: the nested types are the interface a reader opens catalog.hh for.
2. Move them in a step 1b: each type's head becomes `template <class Index, class H> class
   catalog<Index, H>::pin {`, its body moves unchanged, `class pin;` stays. Proof: plain
   split_check against 1a, whose only errors are the 5 heads (answered by name), plus fnsizes.
3. Extend split_check's member mode to nested class definitions (a cpp-libraries quest), then 2.

## Step 1: the map

Class body on main (strata:cfa20b41): catalog.hh lines 259-2661, 2,403 lines. outline.py moved
115 multi-line member definitions (1,783 lines); 53 one-line definitions stay in the class.
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
| A2 | step 1 at strata:bb8f4834 (code = the move commit 94f6fb5d): members 261124-120328-f817d0 (passed: 115 definitions moved, 0 errors, 0 warnings); fnsizes 261124-120331-87d38d (queued) |
| A3 | step 1: selfcontained 261124-120349-3bdf6e (queued; 261124-120334-519a1c was a malformed command, cancelled) |
| A4 | |
| A5 | strata docs/mechanisms/catalog-layout.md, docs/source-layout.md (strata:bb8f4834, step 2 section after) |

Step 1 commits: strata:94f6fb5d (the move), strata:bb8f4834 (docs).

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
- R5 Step 2's subobject is `catalog_detail::s3fifo<Slots>` in catalog_s3fifo.hh (included before
  the class, as index_ledger.hh): queues, ghost, small cap, mutex, and the walk (`evict`) with its
  limits and round bound. Each candidate's fate stays the catalog's (a lambda deciding from the
  word: used, closed, kept), as do the frequency writes, `to_ghost` and the stats counts.
  policy_limits/policy_result/queue_sizes_t become aliases of catalog_detail types (same names,
  same fields). The links stay in the slot (q_next doubles as the free list's link; slot stays
  184 B), reached through `policy_slots`. Two orderings move by a hair, not in effect: the ghost-hit
  count is now taken after the policy's mutex is released, and requeue reads the word after the
  queue check instead of before (a load, no side effect).
- R6 docs/mechanisms/catalog.md (outside the file set) still names `policy_mu_` (lines 230, 248):
  now the policy subobject's mutex. Left for its owner; the lead may grant the file.
- R4 A fifth header, catalog_table.hh, holds what every mechanism shares (construction and
  teardown, the slots' chunks, chores and timers, the host's defaults, the call guard): none of the
  four mechanisms owns them. catalog_observe.hh is 93 moved lines (117 with its head), under
  SPLITTING.md's ~100-line floor; kept, as the quest names observation as a mechanism and its
  readers (tests, T4) are not the transitions'.
