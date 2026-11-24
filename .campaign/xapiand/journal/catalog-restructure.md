# catalog-restructure: journal

State: waiting on jobs 261124-120331-87d38d 261124-121317-b85f6c 261124-121319-6bfb2b 261124-121322-2dae3d
261124-121324-7a97a8 261124-121326-09df74 261124-121329-8d8400 261124-121332-b58fd5 261124-121334-c69f52
261124-121336-5fe8af 261124-121344-f10263 261124-121346-26182f 261124-121400-0da3d7 (pushed at strata:a4223957;
members 261124-121400-74b161 at the head is expected red: step 2 changes code; step 1's proof is
261124-120328-f817d0 at strata:bb8f4834)
Quest: .campaign/xapiand/quests/catalog-restructure.md (the lead's; never edit it)

Code: strata worktree /Users/kronuz/code/Kronuz/strata-wt/xapiand/catalog-restructure, branch
quest/xapiand/catalog-restructure. Scratch: ~/.scratch/xapiand/catalog-restructure/ (the outline
map, build logs).

## Doing

Nothing: evidence queued at strata:a4223957; then a correctness review.

## Next

1. Read the jobs; answer any fnsizes difference; the lead rules on the nested types (below).

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
| A1 | step 2 at strata:a4223957: build-test 261124-121400-0da3d7, TSan on the catalog tests (12 targets, ctest -R catalog) 261124-121344-f10263, CI on the push of a4223957; local Debug run of every catalog test passed at step 2 before the commit |
| A2 | D180 (step 1's members and fnsizes proofs at strata:bb8f4834 are listed there) |
| A3 | selfcontained over every catalog_*.hh at strata:a4223957: 261124-121346-26182f (it superseded step 1's 261124-120349-3bdf6e; 261124-120334-519a1c was a malformed command, cancelled) |
| A4 | catalog_policy_test's named cases at a4223957 (in build-test, registered as catalog_policy_<case>); mutants of catalog_s3fifo.hh, each a commit on mutant/catalog-restructure/<name>, each job a full build of catalog_policy_test then the named case failing (--fails-without): push-at-head (fifo_order) 261124-121317-b85f6c, ghost-ignored (ghost_to_main) 261124-121319-6bfb2b, small-first (pick_queue) 261124-121322-2dae3d, used-stays (evict_order) 261124-121324-7a97a8, no-round-bound (kept_round) 261124-121326-09df74, transitions-ignored (limits) 261124-121329-8d8400, visits-ignored (limits) 261124-121332-b58fd5, requeue-unchecked (requeue_refused) 261124-121334-c69f52, unlink-keeps-tail (unlink_any) 261124-121336-5fe8af. The catalog's behavior cases are unchanged (lock-step, the D6 scenes) |
| A5 | strata docs/mechanisms/catalog-layout.md, docs/source-layout.md (strata:bb8f4834, a4223957) |

Commits: strata:94f6fb5d (step 1, the move), bb8f4834 (its docs), 702fb47a (step 2), a4223957 (its docs).

## Result

Step 1 (strata:94f6fb5d): catalog's 115 multi-line member definitions moved out of the class body
into five headers with outline.py: catalog_table, catalog_pin, catalog_transitions (the state
machine, whole), catalog_policy, catalog_observe. Class body 2,403 -> 734 lines; catalog.hh 2,663
-> 1,002. Proof: members 261124-120328-f817d0; fnsizes pending.

Step 2 (strata:702fb47a): `catalog_detail::s3fifo` (catalog_s3fifo.hh, 162 lines) owns S3-FIFO's
queues, ghost, cap and mutex and decides order, ghost-to-main, the requeue's refusal and a pass's
walk under the limits; the catalog keeps each candidate's fate. Eight named unit cases over a
fake link table; nine mutants. Body 727 lines.

For the reviewer (the quest's questions): the policy's view of slot state is `policy_slots`
(catalog.hh): gen and queueable read the word under the policy's mutex exactly as
policy_requeue and pick did; `evict`'s order of decide, requeue and the round check is the old
loop's (catalog_policy.hh, catalog_s3fifo.hh). Pin lifetimes and the transitions' orderings are
untouched by step 2, and moved verbatim by step 1.

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
