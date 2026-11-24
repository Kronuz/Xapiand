# catalog-restructure review: correctness

Commit: strata:218e92dba72e
Verdict: APPROVE
Date: 2026-11-24

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

218e92db's tree equals a4223957's (`git diff a4223957 218e92db` is empty), so the jobs at a4223957
stand for it. 94f6fb5d..bb8f4834 and 702fb47a..a4223957 touch only docs/. Landing still needs CI
at 218e92db, which the lead checks; this review does not cover it. members at the head is red by
design (step 2 changes bodies); that is the owner's call.

## Evidence I relied on

- Step 1, the move (strata:94f6fb5d; jobs at bb8f4834, whose code is the same):
  - members 261124-120328-f817d0: 115 definitions moved, 0 errors, 0 warnings.
  - fnsizes 261124-120331-87d38d: the 12 catalog test binaries, Release+LTO, main against bb8f4834,
    with "no function changed size". The rows carry names and sizes, so the set of emitted symbols
    is identical too.
- Step 2 (strata:702fb47a, jobs at a4223957):
  - build-test 261124-121400-0da3d7: 138 of 138 tests pass.
  - TSan 261124-121344-f10263: `ctest -R catalog`, 26 of 26 pass with no ThreadSanitizer report.
    The run includes catalog_stress, catalog_policy (lockstep and the D6 scenes), group_catalog
    and the eight named policy cases.
  - selfcontained 261124-121346-26182f: catalog.hh and all six catalog_*.hh compile alone.
  - Mutants 261124-121317-b85f6c, -121319-6bfb2b, -121322-2dae3d, -121324-7a97a8, -121326-09df74,
    -121329-8d8400, -121332-b58fd5, -121334-c69f52 and -121336-5fe8af. Each log shows the mutated
    header rebuilt and `catalog_policy_test` linked, then the named case failing on a value CHECK
    and exiting 1. A crash, a timeout or exit 2 (an unknown case name) does not count as a kill
    here.

## What I checked myself

1. **Step 1 does not change behavior (question 1).** Beyond members:
   - A line-multiset diff of the base class body (cfa20b41 catalog.hh:259-2661) against step 1's
     six headers, with heads excluded. Script:
     `.scratch/xapiand/review-catalog-restructure-correctness/move_lines.py`. Every body line
     reappears. The only base lines that do not reappear are three heads with trailing comments
     (waiters_of, key_at, overload).
   - Order: the mechanism headers are included right after the class closes (catalog.hh:990-994),
     with nothing in between. Non-dependent names in the moved bodies therefore bind against the
     same namespace scope as before. Each header includes catalog.hh under `#pragma once`, so a TU
     that includes a piece first still sees the same order.
   - Hidden friends: the moved code has none. The only friends are `friend class` declarations, and
     they stayed in place.
   - Templates: there is no explicit instantiation or specialization of `catalog`. Every moved
     member is still a template member, so ODR and inline linkage are unaffected. The only effect
     of losing the implicit inline is the inlining hint, and fnsizes shows that hint changed no
     function. There are no visibility flags in the build. Default arguments stay on the
     declarations.
2. **The s3fifo invariants (question 2).** I compared 702fb47a against the removed members, line by line:
   - push_tail and unlink_locked are the old push_tail_locked and unlink_locked, rewritten over
     `Slots`.
   - pick: same small/main choice (`size_[small] > small_cap_ || size_[main] == 0`), same `round`,
     same gen read under the mutex.
   - requeue: the same four conditions under the mutex. Only the order changed: the queue test now
     comes before the word load, and that load has no side effect.
   - enqueue: same ghost take and queue choice. The hash is still computed before the lock.
   - cap(): same tenth and half, still assigned in the constructor body without the lock.
   - evict: same loop condition and order (visits, transitions, then over), same `++visited`.
     Each old branch maps to one visit value. A used candidate is requeued to main and resets the
     skip count. A closed one resets it. The probe_at-busy, hot-demote and refused-close
     candidates are requeued to their own queue and increment it, with the same `> round` break.
     The order of decide, requeue and the round check is the old loop's.
   - The ghost's mutex covers the same state: the queues, sizes, ghost, and the q/q_prev/q_next
     links. `q_next`'s free-list use is unchanged (catalog_table.hh:93-107) and touches only slots
     in no queue.
3. **Lock discipline (question 2).** Still one mutex. `decide` and `over` run outside it, as the
   old loop's body did, so no lock nests inside it that was not there before. Only two things moved
   relative to the lock:
   - The ghost-hit counter is now incremented after the mutex is released. It is a relaxed
     counter, read only by stats().
   - The order of the word load and the queue test inside requeue, which is still under the mutex.

   Reads outside the lock are the same as before: small_cap() and ghost_capacity() read values set
   in the constructor. I found no gap that warranted a TSan probe of my own; the TSan job above
   covers the catalog's concurrent tests.
4. **The tests (question 3).** The eight cases check order, membership and counts by value:
   - FIFO order, head to tail with back links.
   - pick_queue's `>` boundary at the cap. A `>=` mutant would pick {0,1,5,2}, not {0,5,1,2}.
   - The ghost's capacity and small cap: 50 and 10.
   - The visited and posts counts.
   - Each requeue refusal separately.

   A mutant I would have added, requeuing a kept candidate to main, is still killed by
   kept_round's `order(1)` check.
5. **The split (question 4).** I read it against cpp-libraries docs/SPLITTING.md:
   - The cuts follow the quest's mechanisms.
   - The transitions are kept whole, and catalog-layout.md argues why: each begin and its end, and
     each booking and its release, would otherwise sit in different files.
   - No header is a fragment: each compiles alone and holds only whole out-of-class definitions.
   - The nested types stay in the class, as the lead ruled on 2026-11-24.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| N1 | no | nit | strata docs/mechanisms/catalog.md:230 and :248 still name `policy_mu_`, which no longer exists: the rename in c2bcf7b4 was held back by 218e92db. A reader looking for the mutex there finds nothing. | `git -C strata grep policy_mu_ 218e92db -- docs/mechanisms` | Land the held rename (R6) right after this quest, as 218e92db plans. |
| N2 | no | nit | catalog_table.hh is a "what every mechanism shares" header: construction and teardown, chunks, chores and timers, host defaults, test hooks. It is the one piece that is a remainder rather than a mechanism. Acceptable under R4, but a maintainer may name it for what it does (the lifecycle). catalog_observe.hh, at 117 lines, sits at SPLITTING.md's ~100-line floor. | catalog-layout.md table; `wc -l` | Owner's taste; no change needed. |
| N3 | no | nit | The comments in catalog_s3fifo.hh:36 and catalog-layout.md say the links are ones "this class alone writes". `q_next` is also the free list's link, written by alloc_slot and free_slot while the slot is in no queue, which catalog.hh:320 does say. | catalog_table.hh:93-107 | One clause: "while the slot is queued". |
| N4 | no | nit | The reordering noted in R5 (the ghost-hit count after the unlock) means stats().ghost_hits can briefly trail a queue_sizes() read on another thread. Nothing reads the two together under the lock. | catalog_policy.hh policy_enqueue | None. |

## Not run

- CI at 218e92db on Linux, FreeBSD and the sanitizer matrix. It was running; CI owns that result.
- No TSan probe of my own: reading the code found no new lock order or unlocked read, and the
  TSan job covers the catalog tests at this tree.
- I did not re-derive step 1's members check beyond the line-multiset cross-check.
