# cold-tier-step2 review: correctness

Commit: strata:58812244
Verdict: CHANGES
Date: 2026-11-11

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- build-test 261111-034631-5cd30b at strata:9621595f (58812244 is docs only over it): the suite,
  lazy by default, with `reads_pin`, `pin_after_promote`, `damage_cases`, `open_checks` and
  `apply_atomic_test`'s section cases.
- 261111-035757-eb666b (162 of 162 mutants killed, catalog_test at 1,000 seeds) and
  261111-035757-1edccb (the five TSan-only races, 5 of 5): I read `tools/mutants/cold_tier.py`; an
  edit that does not apply or does not build is "not run" and fails the job, never "killed", and
  the unmutated tree must pass first. The logs end "162 of 162 killed; ... not run: none".
- 261111-035757-bf4776 (K3: each section eager and lazy, the three rollback reverts killed).
- CI run 42993976189 at strata:948c0869 (TSan, ASan+UBSan, every platform).
- The T rows' logs: T2/T2d 261111-085642-fdea98 (read whole), T3 261111-034629-8f5208 (read
  whole, per-sample columns extracted), and the doc's numbers for the others.

## What I checked myself

- Risk 3 (failure paths under the catalog): probe `damage_probe.cc`, run by `probes.sh
  damage_probe` (job PROBE_JOB), both under
  `.scratch/review-cold-tier-step2-correctness/`. A segment missing, resized or with a damaged
  `key_index`, through `pin_request` on a `world<inline_executor>`, lazy and eager, with and
  without `defer_io`: after the failure no pin, the kernel model's descriptors and maps and the
  budget's at zero (or equal while the slot is open), the slot reopening once the file is back,
  and a drain releasing everything. PROBE_RESULT
- Risk 1 (reads against tier transitions): read `open_segment::seg()`/`map_body()`
  (`index.hh:2523-2560`: a CAS install, the loser's body freed, a failed marker), `demote()`/
  `promote()` (`index.hh:1089-1135`), `close()`/`try_finish_close()` (`index.hh:649-700`: a
  closing index waits for every view, I7), and `reads_pin`/`pin_after_promote`
  (`test/catalog_wake_test.cc`). `reads_pin` asserts values (both gets, the miss, a scan of 6,
  zero file calls, zero unleased maps) and has a live control (the unpinned read of the second
  owed index maps and counts). The only production change in the quest is the default flip
  (`index.hh:377-382`, `:532`); a lazy map after `demote()` happens in the eager catalog stress
  too (twins have no body), so TSan's stress covers lazy reads against demote. No defect found.
- Risk 2 (T3's 11 descriptors): `sampler::now` (`catalog_tiers.cc:455-460`) reads the budget,
  then `take()` (task_info, `PROC_PIDLISTFDS`, then the whole `mach_vm_region` walk,
  `meters.hh:55-75`), then the budget again. The per-sample excess in 8f5208's log is 3 (the stdio
  descriptors) in 193 samples and at most 46 in the single-read column; the bracketed maximum, 11,
  is a transient descriptor reserved after the first read and released before the second: at most
  one per concurrent open or close (eight chores), plus stdio. `fd-budget.md`'s storm probe
  (2b0af2) bounds the tight bracket at 3. It does not grow with index count; it grows with the
  chores' concurrency and the width of the bracket. The budget's hard caps are enforced by CAS
  before the kernel call (`resource_budget.hh:209-215`).
- Risk 4 (A5): read fdea98's log: `wakes 500` in every warm-defer rep, `opens 500` in every cold
  rep, so the rows time what they name. Warm defer's slowest p50 (85 µs) is below cold's fastest
  (212 µs plain, 264 µs defer) on d1s1, and likewise on the other two fixtures from the doc's
  table. See F3 and F4 for what the doc says beside it.
- Risk 5: the new tests assert values (`reads_pin`, `damage_cases`, `section_attempt`,
  `bound_and_backoff`'s edges, T5's counters by name). `bound_and_backoff` no longer drives a bound
  close to `free` itself, but `catalog_wake_test.cc:606`, `:637` and `:675` still do.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | gap | T3's soft-cap check runs at a different point than the design's target, and at the design's point the row fails for hot. The design asks for the counts "within one sweep pass of the soft caps after the touch ends" (`cold-tier.md:661`); the harness checks once, 3 s after the Zipf requests end (`catalog_tiers.cc:575`, `:591`; `catalog-tiers.md:30`). In 8f5208's 237 samples after the touch, hot exceeds soft_hot + 16 (516) in 159, up to 811 (62% over 500), and open exceeds 2,016 in 202, up to 2,279. The doc reports PASS without saying it reads the target differently. | 261111-034629-8f5208 log, `t+73s` onward | Either the harness judges every sample after the touch (the design's text) and the row reports what it gets, a miss going to the lead; or the lead rules the end-of-run reading and `cold-tier.md`'s T3 says so. Rerun not needed for the second. |
| F2 | yes | gap | A4 is marked met while T1's and T2's targets are not judged and one is missed. T1 cold is 292 B per index against "≤ 16 B" (`catalog-tiers.md:84-85` says so); T1 warm's target (the sum of its measured pieces plus 7%, 0 maps) is not computed (its 0.03 maps per index equal cold's, so likely the harness's own); T2 hot on d1s1 is 1.3-2.1 µs p50 against main's 0.9-1.5 (target within 5%), and T2 cold's "within 10% of the re-measured open plus get" is not judged (both ranges span 0.2 to 21 ms). T1 hot is met through K6 (A9), the design's procedure (`idle_index ... tier hot`), not through the catalog harness (+10% heap, +3 to +9% RSS). | `catalog-tiers.md:72-118`; `cold-tier.md:657-658` | One verdict line per T1 and T2 target in `catalog-tiers.md` (met, missed, or not judgeable at this noise and why); the misses go to the lead, who rules them accepted for step 2 or re-targets the design. |
| F3 | no | gap | The `promote()` figure contradicts the wake it is part of. `promote_probe` reports 94 to 103 µs p50 on d1s1 (and 56-63 µs per segment, so 560-630 µs, on d1000s10), while the whole warm-defer request, a pool hop, `promote()`, the resume and a `get`, is 47 to 85 µs (351 to 394 µs on d1000s10) in the same job. The doc presents the probe as promote's cost above the 33 to 50 µs estimate (`catalog-tiers.md:112-114`); the probe (`catalog_tiers.cc:373-394`, a fresh open, `demote()` and `promote()` back to back) measures something else as well. | 261111-085642-fdea98 | Say the end-to-end wake bounds `promote()` from above, or find what the probe adds before quoting it against D3's estimate. |
| F4 | no | gap | A5's margin rests on a cost the journal proposes to remove. Cold's p50 is 20 to 41 ms because every open syncs twice (journal finding); in the pairs where the syncs ran fast, cold was 212 to 1,097 µs and the margin 2.8 to 5.3 times. If the lead takes the sync-deferral finding, T2d must be rerun before D3's latency sentence stands on it. D3 still says "macOS's map is unmeasured" and "T2d decides the latency" (`cold-tier.md:198-201`) though T2d has now measured it. | fdea98, 609714, f9dbcf; `cold-tier.md:198-201` | Tie the T2d row to the sync finding in `catalog-tiers.md`; update D3's two clauses with T2d's macOS result. |
| F5 | no | nit | The T3 sampler's bracket holds the whole `mach_vm_region` walk between its two budget reads, so the "at most 11 beyond the budget" is the walk's width times the open and close rate, not process descriptors as `catalog-tiers.md:125-127` says (the steady excess is 3, stdio). | `catalog_tiers.cc:455-460`, `meters.hh:55-75` | Read the descriptors with a tight bracket (budget, `PROC_PIDLISTFDS`, budget) and the regions after; state the bound as concurrency of opens and closes. |
| F6 | no | nit | T5's counters fail the run when a timing-dependent counter is not met within eight repeats (`catalog_stress_test.cc:1015-1020`), where the campaign's proof standard says a timing-dependent coverage check reports and never fails. A1 asks for each counter at least 1, so this is the quest's choice; it is a CI flake risk on a slow or fast runner. | `catalog_stress_test.cc:1015-1020` | Keep, or report the shortfall and require the counters from the deterministic tests that force each one (catalog_test's schedules, catalog_wake_test). The lead's call. |
| F7 | no | nit | T2d runs 500 indexes per job where the design names 100,000 (D59's scaling); at 500, every fixture file stays in every cache, so the cache-warm rows are the easy case. | `catalog-tiers.md:93`; `cold-tier.md:659` | Say so beside A5's verdict; the full-scale milestone row settles it. |

## Not run

- No TSan or ASan build: the concurrency claims rest on CI's run at 948c0869 and 1edccb.
- No rerun of any T row; F1 to F5 are read from the evidence jobs' own logs.
- Linux, cache-dropped and full-scale rows (milestone checks by the quest's D57).
- I did not review `docs/reviews/*`, the design's revision history, or prospect's and Xapiand's
  seams (A10) beyond the K7 listing's existence.
