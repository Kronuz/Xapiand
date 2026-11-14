# flush-within-budget review: correctness

Commit: strata:ecd11e7421e1dab0eb3104584eedcbbb418891b0 (ecd11e74; io_spill at strata:92408e2e)
Verdict: APPROVE
Date: 2026-11-13

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- 261113-182303-c8b3af (A1 at ecd11e74, prospect 1a122b80 pinning strata ecd11e74, verified):
  1,102,112 KiB peak RSS vs 1,170,329 limit (1.036x), 2 segments. Its mem.log: the peak is in
  the first flush, budget at 1,051 MiB, RSS 51 MiB above the charge.
- 261113-171217-033927 (main, 1.52x, 2 segments) as the baseline; prospect be7a2c75 differs from
  1a122b80 only in the strata pin (main = 397c02d5, the merge base).
- 261113-182306-1d133e: suite 75/75 at ecd11e74, Release, including index_test's flush_spill()
  and flush_memory.

## What I checked myself

Scripts in `.scratch/review-flush-within-budget-correctness/` (rerun with `campaign job submit
--quest flush-within-budget -- bash <script>`). The VM runs use the binaries the A1 jobs built
(imem-be7a2c75c536 = main, imem-246bafe25b41 = 92408e2e, imem-1a122b800775 = ecd11e74).

1. A2 at segment level, `a2_digest.sh`, job 261113-192119-2c7603: 1M keyed, identity seed 7,
   merges off, main vs 92408e2e. sha256 of manifest-1, seg-1.sx and seg-2.sx are identical. The
   spill changes where the bytes wait, not the bytes. The same runs reproduce A1's numbers
   (1.519x, 1.292x).
2. io_spill failure paths beyond the suite, `probe_spill.sh` + `rv_probe.cc`, job
   261113-192511-3972b1 (fresh checkout at ecd11e74):
   - P1: a read error on read-back throws EIO, leaves no spill file and holds no charge.
   - P2: the segment's output failing mid-copy (ENOSPC) leaves no spill file.
   - P3: a failed remove after a full copy throws EIO and leaves the file.
   - P4: a same-named stray file makes create_exclusive fail with EEXIST, and the stray stays.
3. The reserve on other shapes, `shapes.sh`, job 261113-192122-be51c1, and `bigdocs_memlog.sh`,
   job 261113-193108-a47ea4 (peak RSS / budget, segments, main → ecd11e74):

   | shape | budget | main | ecd11e74 | segments |
   | --- | --- | --- | --- | --- |
   | 500k docs | 520 MiB | 1.536x | 1.057x | 2 → 2 |
   | 100k docs of 2.7 KB (`corpus-many-8x100000-4`) | 160 MiB | 1.599x | **1.293x** | 2 → 2 |
   | same | 320 MiB (no flush before commit) | | 0.919x | 1 |
   | ~5M (`corpus.json`) | 2,056 MiB | 1.566x | 1.090x (20 MiB margin) | **3 → 4** |

4. Code reading:
   - Spill names: `spill_name(id)` takes ids from `cur_.next_segment++`, which is never reused,
     and flush_documents gives each run its own `ids[k]`. recover() counts `.spill` names into
     `max_segment`, so `next_segment` starts above any crash leftover and the leftover is
     tracked as an orphan (`index.hh:5327`). A new spill can't collide with a crash leftover or
     with a concurrent flush or merge.
   - The io_spill destructor runs before flush_active's catch, so the spill is gone before the
     ENOSPC cleanup.
   - flush_documents now calls `io_->create_exclusive` and `remove` from its run threads. Merges
     already call the same IO from background threads (merge_spill), so this is not a new
     concurrency contract.
   - The pressure branch of needs_flush() is byte-for-byte as before. Only the share comparison
     moved to `buffer_limit()`.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | no | gap | `flush_working_pct = 26` is not load-bearing beyond the gated shape. With 2.7 KB documents at a 160 MiB budget, peak RSS is 1.29x (main 1.60x); at 5M it is 1.090x, 20 MiB under the bound. The overshoot is memory the budget never charges, not the flush. In the big-doc run the budget's charge peaks at its capacity (159.8 MiB) while RSS reaches 212 MiB. After the first flush, the heap holds 78 MiB in use against a 21 MiB charge (state outside the buffer, likely the published segment's in-memory structures for keyed lookups). An RSS-fitted percent of the buffer stands in for an uncharged cost that does not scale with the buffer. The answer to risk 1: the accounting gap is the defect, and the constant hides it at one shape. | jobs 261113-192122-be51c1, 261113-193108-a47ea4 (`mem.log` columns rss_kb, budget_used_kb, heap_inuse_kb); `index.hh` `flush_working_pct` | Follow-up quest, outside this one's contract (budget API unchanged): find and charge the uncharged resident memory, then size the reserve from the charged working set (or `pressure(reserve)`), with a probe on the big-doc shape at a small budget. Now: say in the `flush_working_pct` comment and in docs/perf/flush-spill.md that 26% is fitted to the 1M keyed RSS and covers uncharged memory, and give the big-doc and 5M rows. |
| F2 | no | nit | The reserve moves segment counts at 5M (3 → 4), as an earlier trigger (capacity/1.26) must. A1 holds at 1M (2 → 2), and the lead's Decisions anticipated this. Segments at ecd11e74 also differ from main in their boundaries (seg-1 618k vs 854k docs), so "byte-identical" (A2) holds for the spill change, not for the commit as a whole. | job 261113-192122-be51c1; sections.txt of 261113-182303-c8b3af vs 261113-171217-033927 | Record both in docs/perf/flush-spill.md. |
| F3 | no | nit | A failed remove of the spill after a full copy fails the flush with EIO (P3). flush_active's non-ENOSPC path then also leaves the partial `seg-<id>.sx`. Both are orphans the next open removes, and merge_spill has the same shape. A read or write EIO on the spill likewise leaves the segment file (an existing behavior of that path). | job 261113-192511-3972b1 P3; `index.hh` flush_active catch | Optional: treat the remove after copy as best effort, as the destructor does. |
| F4 | no | nit | There are now three spill-to-file classes: `file_spill` (section_io.hh, prospect's tests only), `merge_spill` and `io_spill` (index.hh). That cuts against one mechanism per thing. The contributor ruled this out of scope and flagged it as a follow-up. | `index.hh:232`, `index.hh:3293`, `section_io.hh:136` | Follow-up: one IO spill with a write policy (plain vs merge_file). |
| F5 | no | nit | The spill file is not admitted to `opts_.storage`. Up to a positions section's size (284 MiB at 1M) of transient disk goes unaccounted, so a full disk now surfaces as ENOSPC → space_error instead of the storage budget's refusal. This is a design and durability call, stated in the journal. | journal "Rulings I made" | The durability or design lens decides. |
| F6 | no | nit | docs/perf/flush-spill.md has no ecd11e74 row (1,102,112 KiB, 1.036x, 2 segments, job 261113-182303-c8b3af). | `docs/perf/flush-spill.md` | Add the row. |
| F7 | no | nit | The reserve covers only the share trigger. On a node of many indexes, flushes still start from the pressure path at a full budget, with the working set on top (the risk 4 "unchanged" is confirmed). The many-indexes overshoot stays as it was. | `index.hh` needs_flush() | Note it for the F1 follow-up. |

## Not run

- No flush with `flush_threads > 1` and a positions layout (A1 and my runs are single-thread).
  Concurrent io_spill create/remove from flush_documents' run threads is argued from merges'
  existing use of the IO, not observed.
- No index-level fault injection into a prospect flush (strata core has no layout that writes a
  deferred section). The failure paths are proven on io_spill itself (suite and P1-P4) plus code
  reading of both flush callers.
- No mutant job showing flush_spill()'s fault checks fail without the destructor's remove.
- No Linux CI rerun, sanitizers or macOS. A4 rests on the suite job and the reported green CI.
- No heap profile naming the uncharged memory in F1.
