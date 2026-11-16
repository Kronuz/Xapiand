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
  damage_probe` (job 261111-105742-47e3b6, exit 0), both under
  `.scratch/review-cold-tier-step2-correctness/`. A segment missing, resized or with a damaged
  `key_index`, through `pin_request` on a `world<inline_executor>`, lazy and eager, with and
  without `defer_io`: after the failure no pin, the kernel model's descriptors and maps and the
  budget's at zero (or equal while the slot is open), the slot reopening once the file is back,
  and a drain releasing everything. Result, 12 cases, 0 failures: a missing or resized file fails the open with D11's message ("missing file", "file size differs from the manifest") in every mode, leaving the slot `failed` with 0 pins and 0 descriptors and maps in both the kernel model and the budget, and the restored file reopens (2 fds, 3 maps, equal on both sides). A damaged `key_index` with lazy segments opens; the two keys in the damaged segment throw `corrupt_segment`, the other four read exactly (v2 to v5), the kernel and budget agree (2 fds, 2 maps), and a drain releases everything. Eager or `defer_io` (which forces eager), the open itself fails with the section checksum and holds nothing.
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

## Re-check

Commit: strata:bbef1d4a4115bf9323a0481ac7bcf7fa3cc41b0a
Verdict: CHANGES
Date: 2026-11-11

Range strata:58812244..bbef1d4a. Its code is strata:062182b2's (the later commits change only
`docs/mechanisms/catalog.md` and `docs/perf/catalog-tiers.md`), so suite 261111-151520-66533e
(74 of 74 at 062182b2) and the jobs below stand for the head's code.

### What I checked

- Risk 1, the trigger's concurrency (`catalog.hh:585-593`, `:1374-1377`, `:1403-1435`, `:2393-2404`):
  read, no build. `over_soft()` runs after the shard lock's scope in `acquire_impl` and in `unpin`,
  whose callers (`reopen`, `deliver`, `abandon`, `~pin`) hold no catalog lock there; it reads only
  atomics and `h_`, which nothing assigns after construction, so TSan has nothing new to see.
  `pass_queued_` is set only by `post_pass`'s exchange and cleared by its catch or by `run_pass`
  after the pass, so a throwing post no longer leaves it set (mutant 261111-123743-7732e9 fails
  `catalog_sweep_test.cc:611` without the reset). A post that throws in `over_soft` is swallowed,
  and the next miss, last unpin or period posts the pass. The one window: a miss or unpin that finds a
  pass queued or running posts nothing, and if that pass ends without posting (so `again` is false)
  the excess waits for the next event or the 1 s period. That is the "one sweep pass" of the target,
  not a cap held past it. A pass can only stay queued forever behind a cancelled group, which is
  shutdown.
- Risk 2, T3 against the lead's bound (soft + 16 + the opens in flight): probe
  `.scratch/review-cold-tier-step2-correctness/t3_bound.py` over 261111-151551-cd5253's log, the 259
  samples after the touch (t+36 s on). The bound holds on the net counts of each `flow` line,
  measured against that line's own `opening`: 0 of 259 over for open and 0 of 259 for hot. The net
  maxima are 2,019 and 519, and 4 and 5 samples sit over soft+16, as the doc says. The counted
  `t+` line, read at another instant, exceeds soft+16+8 in 5 samples (max 2,029 at t+246 s, with 24
  closing in flight), which is relief in flight. The harness does not assert the bound: see F8.
- Risk 3, F2: see F2 below.
- Risk 4: mutant 261111-151526-29ff8a (strata:7b126bc1's `catalog.hh`, the one-slot trigger) fails
  only at `catalog_sweep_test.cc:550` (3 over, a pass of 4: posts nothing), and the job requires that
  line, so it fails for the right reason. The test pins both edges for `soft_open`: a band of
  `pass_posts` would fail `:553`.

### Findings

| ID | Blocking | Kind | Status | Finding | Evidence | Fix |
|---|---|---|---|---|---|---|
| F1 | no | gap | fixed | T3 is judged at every sample after the touch, per the lead's 11-11 ruling: MISS before the trigger, then the cause measured and fixed. At the band trigger the net counts meet soft + 16 + the opens in flight in 259 of 259 samples. | `t3_bound.py` over cd5253; `catalog-tiers.md` T3 verdict | None. F8 covers the wording and the harness. |
| F2 | yes | gap | open, narrowed | T1's targets and T2 hot d1s1 now have verdicts, and A4's Evidence row lists their misses. T2 hot on d1000s1 and d1000s10 still has no verdict line, but A4's "met except" list implies it is met. Per pair at p50 it misses 5% in 2 of 3 pairs on each fixture: d1000s1 8.3/9.0 (-8%), 5.9/5.1 (+16%), 9.3/7.6 (+22%) µs; d1000s10 7.6/5.7 (+33%), 7.7/6.4 (+20%), 5.9/5.8 (+2%). At p99 the d1000s1 pairs are +52% and +80% and the d1000s10 pairs +92% and +14%, the others below main. | 261111-085644-609714 log `:4-50`; 261111-035757-f9dbcf log `:4-50`; `catalog-tiers.md` verdicts; journal Evidence A4 | One verdict line each in `catalog-tiers.md` and both cells in A4's "except" list. The lead rules whether d1s1's residual (the request pin by key, about 0.6 µs) covers them. Docs only, no rerun. |
| F8 | no | gap | new | The harness does not judge the ruled bound. `count_mode`'s T3 line still reads only the 3 s-after sample against soft+16, and it returns 0 on FAIL (`catalog_tiers.cc:749-758`), so a T3 job passes whatever happens and the per-sample verdict exists only in prose. The doc's verdict also says "255 and 254 of 259" and "at most 3 past soft+16" where the ruled bound is met in 259 of 259. | `catalog_tiers.cc:749-758`; cd5253 `exit 0` | Judge each `flow` sample's net counts against soft + `pass_posts` + `opening` in the harness and exit nonzero on FAIL, before the Linux and full-scale milestone reruns. State the verdict as MET against the ruled bound. |
| F9 | no | gap | new | T4 has no run at the band trigger. The reruns at the one-slot trigger passed (261111-123758-7ea903: 0.6780 and 0.6776; 261111-123800-7a33ff: 0.8718 and 0.8717, each above the model less 1 point), but the doc says only "T4 reruns at" without their results, and A4's row cites T4 at 9621595f alone. | `catalog-tiers.md` T4 verdict; the two job logs | Record the two reruns' numbers, and say the band trigger posts between off and one-slot (both PASS), or rerun T4 at 062182b2. |
| F10 | no | nit | new | `soft_caps_post` pins the band's lower edge only for `soft_open`. A one-slot trigger on `soft_hot` alone would pass the hot case, and no case exercises the "net of relief" terms (`pending_hot_`, `closing_`, `pending_open_`). | `catalog_sweep_test.cc:555-559`; `catalog.hh:1419-1429` | Optional: a hot case one under the band that posts nothing, and one with a demotion in flight. |
| F11 | no | nit | new | A last unpin now posts to the host's executor from the pin's destructor on the host's thread. Before, it did so only during shutdown (`kick`). A host that drops a pin while holding a lock its executor's `post` takes would deadlock. | `catalog.hh:1403-1414` | One line in `catalog.md`'s host contract: a pin's release may post a pass. |

F3, F4, F6 and F7 are outside the range. The range rewords F5's sentence (`catalog-tiers.md`:
"at most one per concurrent chore (eight), so the bound grows with the chores' concurrency"),
which settles it as written.

### Not run

- No TSan build: risk 1 is settled by reading, since the trigger adds only atomic reads and posts
  outside every catalog lock. CI's `linux-tsan` at the head is still owed by the quest's checks.
- No rerun of any passed job; the T rows are read from their logs.
