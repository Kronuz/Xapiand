# flush-within-budget: journal

State: waiting on jobs 261113-182303-c8b3af 261113-182306-1d133e
Quest: .campaign/xapiand/quests/flush-within-budget.md (the lead's; never edit it)

## Doing

strata:ecd11e74 (pushed): the trigger reserve on top of io_spill. A1 at 92408e2e (io_spill
alone): 1,374,992 KiB peak RSS, 1.29x, 2 segments (job 261113-171233-02281e), vs main
1,616,232 KiB, 1.52x, 2 segments (261113-171217-033927); suite 261113-182306-1d133e (92408e2e: 261113-171054-2e840c green) green.
Measured from that run's mem.log: flush starts at buffer charge 1,040 MiB / RSS 1,071 MiB,
RSS peaks 1,343 MiB: working set 272 MiB = 26% of the buffer (budget charge +209 MiB, 20%).
`buffer_limit()` = min(buffer_share, capacity / 1.26) under a budget (`flush_working_pct`).
Queued: A1 at ecd11e74 261113-182303-c8b3af (prospect pin 1a122b80, branch fwb-a1-head);
suite 261113-182306-1d133e.

## Next

1. Read 261113-182303-c8b3af's a1.txt: peak RSS <= 1,170,329 KiB and segments == 2. If the
   segment count moved, say so in the journal and docs/perf/flush-spill.md; add the row there.
2. Read suite 261113-182306-1d133e (budget tests may depend on the old trigger) and CI on the
   branch (A4).
3. Ask for the correctness review.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261113-182303-c8b3af at ecd11e74 (checks <= 1.1x) vs 261113-171217-033927 at main (1.52x) — pending; 92408e2e alone 1.29x (261113-171233-02281e, failed the bound) |
| A2 | suite 261113-182306-1d133e (92408e2e: 261113-171054-2e840c green): index_test `flush_spill()` compares io_spill's bytes with string_spill's (0, 100 B, 1 MiB, 3 MiB; writes below, at and past the gather); flush_memory_test's pinned segment digests — pending |
| A3 | suite 261113-182306-1d133e (92408e2e: 261113-171054-2e840c green): index_test `flush_spill()` faults (write 1st/2nd before/after effect, create, ENOSPC capacity): throws that errno, no `.spill` left, charge 0 — pending |
| A4 | suite 261113-182306-1d133e (92408e2e: 261113-171054-2e840c green) + CI on the branch — pending |

## Result

strata:92408e2e: `index_detail::io_spill` (index.hh) holds a flush's deferred section: 1 MiB
gathered in memory (charged by capacity), the rest written to `seg-<id>.spill` through the
index's IO, read back by `copy_to()` and removed, also on destruction after a failure. Both
flush paths (`flush_active`, `flush_documents`) use it; `detail::chunked_spill` (section_io.hh)
is gone (no other user). strata:ecd11e74 adds the trigger reserve (`buffer_limit()`, 26% measured). Notes and numbers:
strata docs/perf/flush-spill.md.

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
- Reserve sized from the 92408e2e run's RSS growth (26%), not the budget charge (20%):
  RSS is what A1 bounds, and the flush allocates past what it charges. A fixed measured
  constant, not adaptive per index (the first flush has nothing to adapt from); it applies
  only with a budget and only lowers a share above capacity/1.26. The pressure path is as it was.
