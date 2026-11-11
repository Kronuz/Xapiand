# cold-tier-step2: The cold tier's step 2 finished and gated

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status cold-tier-step2`.

## Outcome

strata's cold tier is complete for the release: the catalog (items 1 to 6 and 5b, landed) passes
its step-2 sanitizer run with counters on every platform, the tier rows are measured against
re-measured baselines (T2d decides D3's latency sentence), step 1's owed criteria are closed, and
`docs/design/cold-tier.md` lands on main at the revision the code implements (road-to-release,
"The strata lane" 2; strata `docs/status/cold-tier.md` and `docs/status/cold-tier-step2.md`).

## Acceptance criteria

Step 2 (strata `docs/status/cold-tier.md` on `design/cold-tier`, "Done criteria for step 2"):

- A1 S9 (item 7): T5's step-2 run under TSan, ASan and UBSan with its counters (pins meeting `opening`, `waking`, `demoting`, `closing`; busy restores; stale handles; failures in backoff; a reposted wake, each at least 1), and CI green on every platform: the `asan` and `tsan` checks and CI
- A2 T6 step 2: 1,000 seeded schedules over every writer of the word with the ledger's S3 to S7 mutants dying; T7 counter fidelity at quiescent points (design T6, T7): jobs
- A3 Idle-index baselines (C2) for T1 to T4 and T2d: the main-built harness measured in the same `strict` job as the quest build, interleaved: the rows' jobs
- A4 T1 memory per tier, T2 first query per tier, T3 count far above the caps (1M indexes, zero `ENFILE`/`EMFILE`/`ENOMEM`/`too_many_open`), T4 Zipf over 1M at least the S3-FIFO model less 1 point (design gate rows): jobs on Linux (VM) and macOS (macOS warm-cache rows gate the quest; Linux and dropped-cache rows run once on main as milestone checks, D57: no Linux host)
- A5 T2d wake cost (CT-R24): warm below cold at p50 and p99 at every segment count, cache warm and dropped, on each platform; where it is not, D3's latency sentence goes (road-to-release "the T2d wake-cost row"): jobs on Linux (VM) and macOS (macOS warm-cache rows gate the quest; Linux and dropped-cache rows run once on main as milestone checks, D57: no Linux host)
- A6 CT-R23's read backstop: reads pin, and a request pin is handed out only after the wake's `promote()` (design ledger log 2026-11-02; S5 of item 4): `catalog_wake_test` in the `build-test` check

Step 1 leftovers (design ledger "Done criteria for step 1"; step 1 landed at strata:66ddb16f):

- A7 K3: `apply_throw_probe`, the allocation sweep and the revert check of CT-R5's rollback pass at the quest head (D57: a job, no rechecker): a job
- A8 K4: the twelve damage cases with lazy segments; a missing or resized manifest-named file fails the open (D11): tests in the `build-test` check
- A9 K6: T1's step-1 rows: hot within 5% of today; demoted with its own target (N1); 1 and 10 segments: `strict` jobs
- A10 [artifact] K7: prospect's construction sites and Xapiand's seam listed, with the pin bump that adopts `seg()`: `docs/status/cold-tier.md`
- A11 K8: CI green on every platform, ASan, UBSan and TSan: CI
- A12 Item 6 of step 1, lazy as the default (`eager_segments` off) with K4's twelve cases and K7's two pin bumps (design ledger, Blocked on): the `build-test` check with `STRATA_EAGER_SEGMENTS=0`

The design:

- A13 [artifact] `docs/design/cold-tier.md` on main at the revision the code implements, with item 5b's revision 8 text and the "Design vs code" items of the step-2 ledger: `docs/design/cold-tier.md`
- A14 [artifact] The step-2 ledger's open questions answered (item 6 seam rulings, Q6): `docs/status/cold-tier-step2.md`

## Contracts

The catalog's host interface as landed at strata:5d742b24 (consumed by Xapiand's `x-pool`); this
quest changes no interface unless a T row forces it, and then names it here first.

## Decisions

- Items 1 to 6 and 5b are history quests (`cold-tier-step2-item1` ... `-item5b`); this quest is
  what the lane's step 2 still owes.
- Task `design` adopts `design/cold-tier` (strata:94d65f0, 17 commits: revisions 6 to 7, the
  re-checks, the X-POOL host rulings); its documents land in strata's own `docs/`.
- Linux runs on the local VM (D24); T3 at 1M indexes needs 12 to 16 GB (design D1).

Lead, 11-11, on the correctness review's F1 and F2 (verdicts at strata:ea54432b):
- T3 soft caps MISS (open above soft+16 in 212 of 248 samples after the touch, max 2,355 against
  2,000; hot max 811 against 500; within only ~282 s later, once requests end, against a 1 s sweep):
  the soft caps are this tier's purpose, so the quest finds the cause before landing (performance,
  at most three experiments) and either fixes it or brings the owner a measured residual.
- T2 hot d1s1 MISS (+56%, +40%, -7% per pair): one exclusive A/B against main with an A/A spread
  decides whether it is real.
- T2 cold d1000s1 and d1000s10: judge only pairs whose opens ran in the same device sync mode; a
  pair split across modes measures the device, not the tier.
- T1 warm heap: measure its pieces (one strict job) and judge the sum plus 7%.
- T1 cold 292 B against 16 B: a recorded residual, the hunt row "cold memory", its own quest.
- `post_pass()` leaving `pass_queued_` set when its post throws stops every later pass: fixed in
  this quest, with a test that fails without it (lead, 11-11).

## May land early

The design documents (task `design`), and tests that change no code.
