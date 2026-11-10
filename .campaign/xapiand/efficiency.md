# Efficiency ledger: campaign skill against the earlier gated pipeline

Started 2026-11-08 21:30 PST, weekly quota 0% (reset 2026-11-16 05:00 UTC). From agent-campaign PR #1
(usage-claude-code) `campaign usage` reads Claude Code; until it merges, run that branch's
`scripts/campaign.py usage`. Two token units: `usage` counts cumulative tokens per request (cached reads
included); a completion notice counts the agent's final total. Compare like with like.

Baseline (the earlier gated pipeline, week of 2026-10-26, from the skill's measured costs): explorers
~85k tokens each; developers 130-460k; reviews 150-400k; a repin 74k fresh against ~500k resumed.
Lead turns are not counted on either side.

Measures per landing: agent tokens, weekly quota points, wall time start to land, review rounds,
rework (fix rounds, relaunches), jobs and their machine time.

## Agent runs

| Date | Quest | Role, model, effort | Tokens | Quota before → after | Wall | Outcome |
|---|---|---|---|---|---|---|
| 11-08 | x-pool | contributor, opus, high | 363k (notice); 44.5M cumulative at 1h15m, 99% cached | 0% → 2% at 22:34 (all work) | 1h27m, 228 tool calls | RESTORE E2E ruled host overload (R1); built map_owed, assertions, clamps, design rev 3, mechanism note; found the E2E checks prove nothing; waiting on 7 jobs; ended at 360k context, so a fresh contributor resumed it from the journal |
| 11-08 | wbuf-inline | contributor, opus, high | 152k (notice) | 2% → 3% at 00:34 (both agents idle on jobs since) | 22m, 69 tool calls | rebased clean onto strata:70df28d; job scripts for every criterion; waiting on 10 jobs |
| 11-09 | wbuf-inline | contributor, opus, high (resumed) | 183k (notice) | 3% → 4% at 01:34 | 7m49s, 20 tool calls | m1 "survivor" was a log overwritten on the case-insensitive disk (M1 vs m1); single-test runner, numbered logs; A8 script fixed; 10 jobs at strata:efc5663 |
| 11-09 | x-pool | contributor, opus, high (fresh, Resume prompt; resumed once) | 78k + 101k (notices) | 4% → 5% at 03:33 (with a third turn, 143k: kill -9 durability probes) | 5m16s + 2m37s + 8m03s, 60 tool calls | merged master (R5); first real strict E2E GREEN (261109-013630-5e7a1c); fixed master's 3 TSan test bugs on the lead's reading of the logs (Xapiand:41dba47c5); 8 jobs queued |

## Lead-only work (no agent)

| Date | Quest | What | Jobs |
|---|---|---|---|
| 11-08 | e2e-harness | The E2E harness fails when it proves nothing (Xapiand:029b324e1) | 2 |
| 11-08 | wbuf-inline | A11: deadline wait (strata:47628b2), repeat harness (strata:c15019c, strata:287b461) | 4 (2 failed on the VM key, rerun); A11 done: branch 0/300, main 49/300 |
| 11-09 | (tool) | rerun bug fixed: `job rerun` moves the `--ref` that holds the quest's code (agent-campaign:0ed7653, dotfiles:ee3d5f7); suite 180 OK, revert check fails as it should | 2 suite runs (light) |

## Incidents

| Date | What | Cost |
|---|---|---|
| 11-08 | Migrated checks built with -j8/-j6 under light holds; load 126 on 16 CPUs blocked Detent's strict jobs | x-pool's first check round (build-test, e2e, asan green; tsan red at 3f4114f) is keyed to the old commands and reruns; fixed in 37 quests (Xapiand:ca85dd0c9 and the others) and in the tool (agent-campaign:aa627ea) |
| 11-08 | Every `e2e` check graded a stale /tmp report as GREEN (newman 4.5 on the jobs' PATH); found by the x-pool contributor | every earlier E2E pass on this host is void; e2e-harness fixes it, 14 quests' E2E checks now set newman 6 and RUN_ALL_STRICT |
| 11-08 | ci-quest-branches (3 quests): my `yaml` check's quoting broke python (SyntaxError), all 3 failed | 3 jobs of ~1 s each; check fixed to `grep -F` and rerun (261109-003516-d9144b and 2 others) |
| 11-09 | Detent's lead reached this lead directly through the new cmux wake (agent-campaign:d15a35d): msg-3 asked and answered in minutes, where msg-1 to msg-3 had waited hours unread | none; this validates the tool fix |
| 11-09 | wbuf-inline's A6 (261109-012440-05ac8b) died 38 min in and read as "the host rebooted": macOS kern.boottime drifts, so the reaper never matched the boot (fixed: agent-campaign:0ca1963, dotfiles:b76959e). Why the runner itself died is unknown (empty log) | 38 min of VM time; resubmitted as 261109-032020-5807c6 at strata:bb2f58d |
| 11-09 | The worker counted `none` jobs (no hold) against light slots: a 1-s yaml check queued behind hour-long builds | fix on agent-campaign main with this hour's push |
| 11-09 | Two `campaign wait`s started two workers at once (pids 63640, 63642): a starter saw a half-made home as stale | fix with the same push: the home goes in place whole; a worker that lost it exits |
| 11-09 | GitHub refused every ci.yml push: the Kronuz token lacked the `workflow` scope (owner refreshed it) | the 3 ci-quest branches waited ~5 h on it |
| 11-09 | In 24 h, Xapiand's local sanitizer jobs took 10.7 h of slot time (31 runs, ASan 6.9 h, TSan 3.8 h), mostly repeating GitHub CI's Linux ASan/TSan; FIFO slots let our 02:44 batch hold every slot while Detent's 33 jobs from 02:55 waited (Detent's median queue wait 21 min vs our 9) | agent-campaign:890090b: `ci:` checks, fair-share worker, shortest first, light holds at CPUs/2 with a memory floor; 19 quests' sanitizer gates moved to CI, macOS builds deferred |
| 11-09 | Our strict wbufrr A/B waited 04:35 to 05:33 for the machine to empty (macOS strict acts as exclusive), and its pending request blocked new light holds: Detent's 39 jobs stalled at load 3 | ~1 h of Detent's queue; cancelled; the worker will hold strict and exclusive jobs back while other campaigns' light work is queued (up to an hour) |
| 11-09 | `campaign job rerun` dropped `after` (Detent's finding, msg-5) | fixed in agent-campaign, pushing with the suite |
| 11-09 | x-pool landed (Xapiand:1c2b3d32b) | C1 re-confirmed at the head after the master merge (probe rerun, 5 min); deferred macOS asan/tsan queued |
| 11-09 | The resubmitted wbufrr A/B (9f1069) drained again under the old worker, 05:50 to 06:34, behind a 2 h Detent mutant job | cancelled; worker restarted 06:36 on agent-campaign:97d27c3 so the drain hold applies; resubmitted as 6b6841 |
| 11-09 | x-pool deferred asan-macos and tsan-macos green at 1c2b3d32b | milestone gap closed |
| 11-09 | Detent's strict A9 (964ef1), submitted 05:35, launched 06:37 by the new worker (its 1 h cap counted from submission) and drained behind a 2 h mutant job, again blocking light holds at load ~4 | tool fix in progress: launch a draining job only when running light jobs end within 10 min; past its cap, start no new light jobs |
| 11-09 | wbuf-inline contributor final turn (rebase, perf note, evidence) | 217k tokens, 9 tool uses, 1.9 min; head strata:3df7af4 |
| 11-09 | wbuf-inline A8 A/B 6b6841 ran in a lull under the drain hold: 91 s, no stall | readrandom 1.06x faster [1.042, 1.091], B/B tie |
| 11-09 | agent-campaign:573b5d4 (worker --restart, deadline-aware drain) landed; worker re-exec'd in place live (pid 5325 kept) | suite 173 OK; three revert checks fail as required |
| 11-09 | My overdue-drain reservation (agent-campaign:573b5d4) held all new light jobs for Detent's 11 queued exclusives, waiting on a light TSan job declared at 6 h: 1 job running with 21 queued, load ~7, from about 08:00 | fix: a drain's wait counts from the last drain's end (turns), and an overdue drain queues its hold at once; agent-campaign:60e4e75, worker restarted in place 08:42, 7 light jobs started at once; ~40 min of host lost |
| 11-09 | wbuf-inline correctness review (opus, high): APPROVE, no blocking, N1-N4 doc nits | 188k tokens, 72 tool uses, 50 min; 2 of 3 probe submissions failed on the probe's own compile errors |
| 11-09 | campaign status 41 s → 3.5 s (Xapiand), 17 s → 3.3 s (Detent) at load 11-17: disk caches for gh and git facts (agent-campaign:0c7ed06) | ~590 subprocesses per call before; output identical |
| 11-09 | wbuf-inline CI linux-tsan cancelled at the 60 min job cap, no TSan report: catalog_s8_smoke takes 41.6 min under TSan on main too | rerun; follow-up: shorten or split that test |
| 11-09 | wbuf-inline CI linux-tsan rerun green in 58 min against the 60 min job cap | catalog_s8_smoke (41.6 min under TSan) is a near-miss for every strata change: follow-up quest |
| 11-09 | Evidence jobs queued with --after never counted (filed under KEY-ORDER, looked up by KEY): wbuf-inline's sweep showed 'ran other code' | fixed in agent-campaign:6105637 |
| 11-09 | Sidebar clutter: hand-set pills never expired, a pill per blocked quest, Detent's pills drawn here by my timing runs | agent-campaign:6105637: report pills expire (--ttl 120 min), quests' pills only active/review; AGENTS.md rule (dotfiles:776b943) |
| 11-09 | wbuf-inline landed on strata main (strata:3df7af4) | A/B at the head 1.048 [1.013, 1.078] (1.05× faster), B/B tie; deferred macOS sanitizers reused from the head's gate jobs |
| 11-09 | `campaign job rerun` of a job whose `--ref` names another repository's branch re-resolves the name in the job's own repository (strata) and fails; the A1/A3 rows also cited a strata-repo job, which never counts for prospect's code | two resubmits (~15 min of light CPU); tool fix to file: rerun keeps refs' pinned commits |

## Agent runs, continued

| 11-09 | x-pool | contributor, opus, high (third and fourth turns) | 143k + 164k (notices) | 5% → 6% at 05:33 | 8m + 4m (+ C1 fix 6m, merge 1m) | durability ruled harness timing (R6, 5/5 at master and branch); ready for review |
| 11-09 | x-pool | reviewer, opus, high (correctness) | 262k (notice) | 5% → | 18m10s, 86 tool calls | CHANGES: C1 blocking (missing-index write 500 instead of 404, found by a probe job); C2-C7 non-blocking |
| 11-09 | wbuf-inline-prospect | contributor, sonnet, medium | 77k | 9% weekly | 13m | repin + 16,000-record budget test; ready for review; lead rebased and re-cited evidence |
| 11-09 | manifest-log | contributor, opus, high (step 3a) | 295k | 9% weekly | 32m | log layer at strata:6a8bb32; folded C4-C6, D41; handed off before 3b |
| 11-09 | manifest-log | contributor, opus, high (step 3b) | 279k | 10% weekly | 1h48m | D42 adaptive deleted set (128 deletes 8.9 s to 0.03 ms local), index on the log, 4 bugs found by ported tests; build-test 70/75 at strata:7ffabab, mutants 23/23 |
| 11-09 | manifest-log | contributor, opus, high (3b fixes, codec retirement, 3c part) | 286k | 13% weekly | 1h16m | 5 red tests were the branch's (main 74/74); fixed, build-test 72/72 at strata:4d2a541; T4 cases 4-9 |
| 11-09 | cjk-words | contributor, sonnet, medium | 78k | 12% | 3m | blocked: no segmenter exists; options led to D43 and cjk-segmenter |
| 11-09 | x-pool-followups | contributor, sonnet, medium (2 turns) | 119k | 12% | 19m | C3, C5, C6, restore wait fixed; jobs queued |
| 11-09 | sortable-proof | contributor, opus, high (2 turns) | 176k | 13% | 31m | A1 2^32 floats 0 failures; CLI 213,491 + 741,467 lines 0 mismatches; docs edit voided all evidence (hunt row) |
| 11-09 | (tool) | general-purpose, opus, high: parallelism changes | 162k | 12% | 21m | agent-campaign:c43dc00, dotfiles:6e0d8af; suite 181 OK |
| 11-09 | manifest-log | contributor, opus, high (3c) | 275k | 14% weekly | 1h02m | R27 fence past the durable boundary (sweep failed 2 points before); T4 case 1, T6 80/80 streams; 7 jobs at strata:9d4630c |
| 11-09 | cjk-segmenter | contributor, opus, high | 87k | 14% | 1h44m | 0 differences vs ICU on zh/ja/ko and a 31.1M-text sweep; 2.18x/1.90x/3.86x faster than ICU locally; 5 jobs at the head |
| 11-09 | perf-watch-peek | contributor, opus, high (2 turns) | 256k | 13% | 1h09m | asking routes 3 to 1 Asio handler per request; timing jobs queued for a drain |
| 11-09 | docs-upgrade-page | contributor, sonnet, medium (2 turns) | 130k | 13% | 14m | upgrade page; A2 round-trip job builds the old binary in the job |
| 11-09 | (tool) | general-purpose, opus, high: slot lock, disk housekeeping, docs-only evidence | 260k | 14% | 1h01m | agent-campaign:9ece90c, 1b2e947, 6579ca6; dotfiles:51ddfe0; sweep removed 35 worktrees |

## Landings

| Quest | Started | Landed | Agent tokens | Quota points | Review rounds | Fix rounds |
|---|---|---|---|---|---|---|
| e2e-harness | 11-08 22:52 | 11-08 23:36 (Xapiand:6e5c3283f, merge c60138850) | 0 (lead) | ~0 | none (low) | 1 (A4 row retagged) |
| ci-quest-branches (Xapiand, strata, prospect) | 11-08 23:16 | 11-09 04:2x (Xapiand:975ca0e3a, merge Xapiand:f77111164; strata:446410f; prospect:b0c8665) | 0 (lead) | ~0 | none (low) | 1 (yaml check quoting); ~5 h waiting on the token's `workflow` scope |
| wbuf-inline-prospect | 11-09 10:5x | 11-09 (prospect:9b88263, Xapiand:2a647a916) | 77k | ~0.5 | none (low; lead read the diff) | 1 (rebase; evidence re-cited from prospect-repo jobs) |
| 11-09 17:2x hourly | — | — | — | weekly 16% used, 0.79%/h | — | launched automata-evaluation (opus, design), phase3-ratchets (opus, performance), sortable-followups (sonnet); disk 134 GiB free; load 17/36/39 |
| barrier-warning-completion | 11-09 | 11-09 17:2x (completion:e49f288, Xapiand:3b9c774b8) | review 160k | ~1 | 1 durability APPROVE, 0 blocking | 0 |
| x-pool-followups | 11-09 14:2x | 11-09 18:xx (Xapiand:05eccd0fb, merge Xapiand:f332e5989) | ~150k contributor + lead | ~1 | waived (normal; mutants + 20/20 restore + CI) | 2 (ready() refactor; mutant retarget) |
| sortable-followups | 11-09 17:2x | 11-09 18:xx (sortable:2e85e15) | 103k | ~0.5 | none (low; lead read the diff) | 1 (artifact rows reformatted) |
| 11-09 18:2x hourly | — | — | — | weekly 18%, 0.84%/h | — | landed docs-site-audit (Xapiand:9cfe372af, merge e57d2d089); agents: docs-site-audit 318k, phase3-ratchets 291k, automata-evaluation 247k, docs-upgrade-page fix 142k, direct-pins 135k; queue 50 (34 Detent dense-pages light); disk 133 GiB; load 39 |
| github-triage | 11-09 18:3x | 11-09 19:1x (Xapiand:00bfe965e, merge Xapiand:9e4edcda6) | 193k | ~1 | none (low; lead read the diff) | 2 (D48 wording; site check restored after a lead error) |
| 11-09 19:2x hourly | — | — | — | weekly 19%, 0.85%/h | — | GitHub: 16 replies posted, 11 issues and 12 Dependabot PRs closed, 5 issues open on planned work; agents: cli-restore-records 183k, query-dsl-fixes 197k, docs-upgrade-page fix 156k, explorer 97k; load 5, disk 119 GiB |
| cli-restore-records | 11-09 18:5x | 11-09 20:0x (Xapiand:09b609049, merge 3542ee9a3, completion b4acd8419) | 183k | ~1 | waived (normal; restore proof + CI) | 1 (rebase; landing interrupted at branch cleanup, completed by rerun, pushed by hand) |
| 11-09 20:2x hourly | — | — | — | weekly 20%, 0.86%/h | — | launched write-path-fixes (opus), docs-upgrade-page resume 72k, cjk-segmenter resume 79k (ready for review), manifest-log resume (opus; CI asan red), cjk-segmenter correctness review; master CI green at b4acd8419; queue 14; load 5.5, disk 121 GiB |
| automata-evaluation | 11-09 17:2x | 11-09 21:xx (prospect:e8e5bd9, Xapiand:3ed4270d3) | 247k + 121k resume | ~1.5 | none (low; lead read the diff) | 0; landed --no-ci (prospect CI ignores docs/**) |
| 11-10 02:1x hourly | — | — | — | weekly 23%, 0.79%/h | — | reviews APPROVE: cjk-segmenter (180k, 0 blocking), query-dsl-fixes (156k, 0 blocking, F2 to owner); resumes: cjk 79k, perf-watch-peek 96k, manifest-log 255k (4 CI causes fixed), direct-pins 60k; launched D49 paired durability (Opus high, Fable max), manifest-log security, perf-watch-peek correctness, Detent async-drivers design review (Opus max); root cause of e2e kills: fixed port 8880 + lsof kill of clients; Kronuz/automata created (automata:328f294); prune freed 7 GiB (94 to 101 GiB); load 31; queue 56 |
| 11-10 D49 trial 1 | — | — | Opus durability ~200k, Fable durability ~227k, Opus security ~188k | — | 3 reviews CHANGES | Opus: 2 unique blockers; Fable: 1 blocker (shared with security), 1 unique nit; both 0 false positives; Fable not ahead on trial 1 |
| Detent async-drivers design review | 11-10 | — | 354k (Opus max) | ~2 | CHANGES, 5 blocking (X1-X5) | sent to Detent; Detent folding them |
| docs-upgrade-page | 11-08 | 11-10 03:0x (Xapiand:2d378acfe) | ~470k over 4 runs | ~3 | none (low; lead read the page) | 3 (CLI restore defect behind it; rebase; e2e evidence voided by tested_docs) |
| query-dsl-fixes | 11-09 18:xx | 11-10 03:1x (Xapiand:a76433b4e, merge b2a428be0) | ~510k (4 contributor runs) + review 170k | ~3.5 | 1 correctness APPROVE + scoped re-review for D50 | 3 (setup bug; reach_of; D50 + F3 test) |
| 11-10 03:2x hourly | — | — | — | weekly 26%, 0.86%/h (ahead of the week: exhaustion 86 h vs reset 138 h) | — | launched docs-site-refresh tasks getting-started (sonnet) and architecture (opus), sortable-encoding design review (opus max); held reference-guide and tutorials tasks for quota; direct-pins waived (mechanical, strong evidence); load 15; disk 95 GiB |
| cjk-segmenter reviews | — | accepted at analysis:fbf5b7d | re-review 199k (opus, resumed) + docs addendum 58k (sonnet) | — | correctness APPROVE; docs-only addendum needed because acceptance binds the head | 1 (F1/F2 fixed) |
| manifest-log fix round 1 | — | — | 270k (opus) | — | 4 blocking fixed, jobs queued | — |
| docs tasks | — | — | getting-started 288k (sonnet), architecture 304k (opus) | — | found HEAD-404-body bug and the WAL crash window (docs only, D13) | — |
| sortable-encoding design review | — | — | 299k (opus max) | — | CHANGES: 2 blocking (marker crash window via executable model, 13,416 misreads to 0; T3 CI home), lead fixed text at Xapiand:8918fb756 | — |
| 11-10 04:2x hourly | — | — | — | weekly 27%, 0.86%/h (exhaustion 85 h vs reset 137 h) | — | nothing launchable (file sets held); no new agents for quota; 49 jobs queued, load 18; disk 94 GiB |
| direct-pins | 11-09 | 11-10 04:5x (Xapiand:19b489f25, merge a37c6ea58) | ~600k over 4 runs (sonnet) | ~3 | waived (mechanical) | 4 (port-collision root cause; 3 rebases) |
| perf-watch-peek | 11-09 | 11-10 05:1x (http:51d153a) | — | — | correctness APPROVE | 1 (http CI needed a manual dispatch: no quest/** trigger) |
| Detent async-drivers re-review | — | APPROVE at Detent:1ece2ff | 108k (sonnet) | — | 14 of 15 closed, X10 partial | — |
| write-path-fixes resume | — | review | 117k (opus) | — | paired correctness reviews launched (Opus high, Fable max: D49 trial 2) | — |
| 11-10 05:2x hourly | — | — | — | weekly 28%, 0.87%/h (exhaustion 83 h vs reset 136 h) | — | started x7-gate (Phase 7c gate, opus, step 1: inventory + functional rows); manifest-log resume (opus) on one surviving mutant; phase6 held; 46 jobs queued, load 4.5; disk 94 GiB |
| cjk-segmenter | 11-09 | 11-10 06:1x (analysis:fbf5b7d) | ~650k over 4 contributor runs + reviews 199k + 58k | ~4 | correctness APPROVE (2 rounds + docs addendum) | 2 (analysis CI needed a manual dispatch) |
| x7-gate step 1 | 11-10 05:2x | — | 205k (opus) | — | inventory: 3 met, 7 gaps → D53 + 4 quests | — |
| manifest-log survivor round | — | — | 134k (opus) | — | R37 survivor killed by a new test; all jobs resubmitted at 8b532c8 | — |
| 11-10 06:2x hourly | — | — | — | weekly 29%, 0.87%/h (exhaustion 82 h vs reset 135 h) | — | nothing launchable; 105 jobs queued, load 16; disk 99 GiB; cross-repo CI-trigger fix denied by the permission system, awaiting owner |
| write-path-fixes paired reviews (D49 trial 2) | — | — | Opus high 187k, Fable max 227k | — | both CHANGES; same two defects, opposite severities; no unique finding | — |
| write-path-fixes fix round 1 | — | — | resumed contributor to 233k (opus) | — | D1 + F1 + N4 + F7 fixed; 1a failed on a wrong test and a fixed-port clash, fixed; jobs resubmitted | — |
| CI quest trigger | — | 82 libraries on main | 69k (sonnet) | — | owner authorized | — |
| 11-10 07:2x hourly | — | — | — | weekly 30%, 0.87%/h (exhaustion 80 h vs reset 134 h) | — | nothing launchable; docs split (D55) and landing on CI; durability concerns into strata design (strata:77adb056); 44 jobs queued, load 11; disk 82 GiB (ccache evicted nothing; slots 60 GiB in open quests) |
| docs-site-refresh (first two areas) | 11-10 03:2x | 11-10 07:3x (Xapiand:c1df4fbe2, merge 1e56dd22b) | getting-started 288k (sonnet), architecture 304k (opus) | ~4 | low: owner read (A7) | 2 (link-check #_top fix; D52 cycle; D55 split) |
| docs-site-refresh-reference (reference guide) | 11-10 07:3x | — | 398k (sonnet; over the 300k target) | — | 8 findings → 5 hunt rows | — |
| x7-gate rows triage | — | — | resumed to 242k (opus) | — | 2 master gaps (DB-TIMER thread, T8 peaks) → D56; 2 harness bugs fixed | — |
| manifest-log round 2 | — | — | resumed to 152k (opus) | — | new A5 test found a real unfenced-after-log-moved defect; fixed R39 | — |
| 11-10 08:2x hourly | — | — | — | weekly 31%, 0.88%/h (exhaustion 79 h vs reset 133 h) | — | nothing launchable; 60 jobs queued behind another campaign's exclusive hold; disk 80 GiB |
| 11-10 09:2x hourly | — | — | — | weekly 31%, 0.85%/h (exhaustion 81 h vs reset 132 h) | — | nothing launchable; a worker stop during the campaign-tool change interrupted 9 jobs (manifest-log 6, docs-reference 2), resubmitted; write-path-fixes 5 of 11 done green; 40 queued, load 4; disk 80 GiB |
