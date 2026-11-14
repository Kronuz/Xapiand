# flush-within-budget: journal

State: waiting on jobs 261113-171054-2e840c 261113-171217-033927 261113-171233-02281e
Quest: .campaign/xapiand/quests/flush-within-budget.md (the lead's; never edit it)

## Doing

Steps 1-3 done at strata:92408e2e (pushed, CI runs on quest/**). Jobs queued:
suite 261113-171054-2e840c (A2, A3, A4); A1 baseline at main 261113-171217-033927;
A1 at head 261113-171233-02281e (after the baseline: both use the gate VM; it fails when
peak RSS > 1.1x of 1,039 MiB; `$JOB_OUT/a1.txt` has peak RSS and segment count).

## Next

1. Read the three jobs (`campaign job show`, `a1.txt`). If A1's head is still over 1.1x, the
   remaining ~120 MiB is builder scratch: reserve that share in `needs_flush` (index.hh, the
   lead's fallback in the quest's Decisions) and rerun A1. Compare segment counts main vs head.
2. Check CI on quest/xapiand/flush-within-budget (Linux) for A4.
3. Write docs/perf/flush-spill.md (strata) with the A1 numbers; ask for the correctness review.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261113-171233-02281e (head, checks <= 1.1x) vs 261113-171217-033927 (main) — pending |
| A2 | suite 261113-171054-2e840c: index_test `flush_spill()` compares io_spill's bytes with string_spill's (0, 100 B, 1 MiB, 3 MiB; writes below, at and past the gather); flush_memory_test's pinned segment digests — pending |
| A3 | suite 261113-171054-2e840c: index_test `flush_spill()` faults (write 1st/2nd before/after effect, create, ENOSPC capacity): throws that errno, no `.spill` left, charge 0 — pending |
| A4 | suite 261113-171054-2e840c + CI on the branch — pending |

## Result

strata:92408e2e: `index_detail::io_spill` (index.hh) holds a flush's deferred section: 1 MiB
gathered in memory (charged by capacity), the rest written to `seg-<id>.spill` through the
index's IO, read back by `copy_to()` and removed, also on destruction after a failure. Both
flush paths (`flush_active`, `flush_documents`) use it; `detail::chunked_spill` (section_io.hh)
is gone (no other user). Numbers pending A1.

## Rulings I made

- The quest names section_io's `file_spill`; it opens paths with fopen, bypassing the index's
  `kronuz::journal::IO` (faultfs in tests, the directory handle in production), so it cannot
  live in the index's directory or take injected faults. The flush spills through the index's
  IO instead, to `seg-<id>.spill`, the name merges use and open() already removes as an orphan.
  `file_spill` stays (prospect's segment_test uses it).
- A spill within 1 MiB never creates a file (many small indexes: no create/remove per flush).
- Not unified with `merge_spill` (index.hh): its writes go through merge_file (rate limit,
  storage reservation, cancellation). One spill with a write policy is a follow-up for the lead.
- The spill file is not admitted to `opts_.storage` (transient, removed before publish); a
  full disk surfaces as ENOSPC from its write, which the flush already turns into space_error.
- Strata core has no layout that writes a deferred section (positions are prospect's search
  layout), so A2/A3 are tested on io_spill itself; the segment-level positions path is
  exercised by A1's prospect run.
- A1 measurement scaffolding: local prospect branches fwb-a1-main / fwb-a1-head (not pushed)
  = explore/index-memory with strata pinned at 397c02d578cc / 92408e2e.
