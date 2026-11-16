# xapiand: migration report

The earlier skill's Xapiand campaign (road-to-release.md, its strata lane and the library work
beside it) mapped onto one campaign `xapiand`, home `/Users/kronuz/code/Kronuz/Xapiand`. Sources:
the Xapiand and strata clones (every branch), prospect, http, completion and sortable from GitHub,
and the laptop extracts. Files: `PLAN.md`, `quests/` (21 live quests), `history/` (94 history
quests and their journals), `map.sh`, `history.sh`.

Rehearsed on throwaway clones with campaign at agent-campaign:18a6db1: `campaign init`, `map.sh`,
`history.sh` (both the default and `ONE_COMMIT=1`), then both scripts again (idempotent: nothing new),
`campaign check` ok, and `campaign map`: 94 of 115 quests landed, the 11 history milestones "done",
Phase 7c 0 of 3, Phase 8 0 of 13, the strata lane 0 of 5. `bash -n` passes on both scripts.

Run order in the home on master: `campaign init --name xapiand --ci github`, then `./map.sh`, then
`./history.sh`. Both need `../strata`, `../prospect`, `../http`, `../completion` and `../sortable`
checked out beside the home; neither pushes.

## Settled during the migration (2026-11-08)

- Question 4: CI at strata:9ce248e failed FreeBSD's `write_buffer` on its first attempt (run
  41944174997) and passed on the second; `wbuf-inline` A11 asks for the explanation.
- Question 16: the baseline finished on 11-04 (harness Xapiand:ec4e6d937, 200,000 documents, every
  step rc 0); results in `~/.scratch/xapiand-baseline/20261105T033044Z/` on the laptop, log in
  `~/code/Kronuz/Detent/.scratch/overnight/done/Xapiand-baseline.log`. Which rows it serves is the
  lead's call.
- Question 17: Detent's campaign records M1 as landed: `m1-pins` at Detent:3e8bdbc and
  `http-coroutines` (off http's offload path) at Detent:d0a5cd8.
- Questions 19, 20 and 22: the migration ran with the libraries beside the home, as one commit
  (`CAMPAIGN_NO_COMMIT=1`), and marked the ledgers of Xapiand and strata as history with a
  "Current campaign" section in `AGENTS.md`.

## Live quests

| Quest | Repository, branch and head | Risk, why | Criteria, source | Status |
|---|---|---|---|---|
| `wbuf-inline` | strata `perf/wbuf-inline` strata:9ce248e | high: write-buffer memory accounting under concurrent readers | 10: strata wbuf-inline ledger, its review CR1, CR2 | planned |
| `wbuf-inline-prospect` | prospect `test/wbuf-inline-flushes` prospect:c0c4260 | low: a test and a repin | 3: wbuf-inline ledger Done 6, Next 1; review NEW-1 | planned |
| `cold-tier-step2` | strata, task `design` = `design/cold-tier` strata:94d65f0 | high: catalog concurrency | 14: cold-tier.md S9, T1-T7; step 1 K3, K4, K6-K8 | planned |
| `hybrid-go-no-go` | strata `design/hybrid` strata:8c539a5 | low: measurement and a decision | 4: D8, hybrid ledger | blocked (parked by the owner) |
| `manifest-log` | strata `design/manifest-log` strata:79beaf4 | high: on-disk commit metadata, decoder | 10: design step 3, steps12 ledger SR2-SR4, D6.3 | planned |
| `x-pool` | Xapiand `phase7c/x-pool-step3` Xapiand:0638e61fd | high: index lifetime, durability | 18: x-pool design XP1-XP7, ledger, step 2 review CR1-CR6 | planned |
| `perf-watch-peek` | http, none | normal: one read per request | 4: road-to-release Perf | planned |
| `x7-gate` | Xapiand, none | high: the gate of concurrency and durability rows | 24: phase7c design T1-T13, C5, eight carried conditions | planned (needs x-pool, perf-watch-peek) |
| `gate-prep-followups` | Xapiand, none | normal: error codes, a bounded view | 8: carried gate-prep and X4-R1 follow-ups | planned |
| `direct-pins` | Xapiand, none | normal: build pins | 4: carried pin-wave condition, D18 | planned |
| `name-index` | Xapiand, none | high: persistent name index | 5: carried "Index patterns at scale" | planned |
| `sortable-proof` | sortable, none | high: a decoder of users' disks | 4: sortable-encoding T2, T4, T10, step 1 | planned |
| `sortable-encoding` | Xapiand `design/sortable-encoding` Xapiand:455d0a447 | critical: freezes stored bytes | 11: sortable-encoding T1-T10, Q1-Q3 | blocked (owner) |
| `license-closure` | Xapiand `fix/license-actions` Xapiand:c47a49bd6 | normal: notices, assets | 9: license audit blocker 3, A3, A9, A17-A19, A21, A26 | planned |
| `docs-upgrade-page` | Xapiand, none | low: docs | 4: road-to-release "Before release" | planned |
| `closing-benchmark` | Xapiand `fix/closing-bench-harness` Xapiand:ec4e6d937, task `design` = `design/closing-benchmark` Xapiand:cf75407dc | low: harness and measurement | 15: design rows T1-T13, harness Next (ledgers' K1 never set) | blocked (owner: AWS) |
| `postings-fuzz-memory` | prospect, none | high: decoder | 4: road-to-release "Before release" | planned |
| `barrier-warning-completion` | completion, none | high: durability | 3: D10, D16, t11-barriers C1 | planned |
| `barrier-warning-strata` | strata, none | high: durability | 4: D10, D16 | planned |
| `barrier-warning` | Xapiand, none | high: durability | 4: D10, D16 | planned |
| `release` | Xapiand, none | critical: rename, published release | 8: road-to-release 8c, strata PLAN Phase 8, A27 | blocked (owner: name) |

road-to-release's steps: X3 land, X3-1, X4, X5, X6 landed (history quests); Perf is prospect's
half landed (prospect:5c98f81) plus `perf-watch-peek`; X7 is `x7-gate`; 8a is `sortable-proof`,
`sortable-encoding`, `license-closure`; 8b `closing-benchmark`; 8c `release`. "Before release":
the barrier warning (three quests), the upgrade page, the postings fuzz target, and Detent off
http's offload path, satisfied (D27). The strata lane: item 5 landed (`cold-tier-item5`), step 2's
items 1-6 and 5b landed (history) and the rest is `cold-tier-step2`, then `hybrid-go-no-go`.

## strata ledgers: history and live

History (work on main): every strata ledger with a history quest (Appendix A). Live:
`cold-tier-step2.md` and `cold-tier.md` (`cold-tier-step2`), `hybrid-search.md`
(`hybrid-go-no-go`), `manifest-log-steps12.md` (history; step 3 is `manifest-log`), and
`wbuf-inline.md` on its branch. Parked by the owner (PLAN Non-goals): `durability-modes.md`,
`peer-transport.md`, `simulator.md`. Live but on no plan: `automata.md`, `bisection-pool.md`,
`queryparser.md` (questions below).

## Judgment calls and open questions for the lead

1. X-POOL is its own quest: road-to-release does not list it, but its ledger is live 7c work, and `x7-gate` needs it (T13, the wide-search pins).
2. `x-pool` adopts `phase7c/x-pool-step3` alone: it contains `phase7c/x-pool` (step 2), so the two branches are one line, no task; its last four commits postdate the ledger and are unverified.
3. `closing-benchmark` takes the harness branch as the quest and `design/closing-benchmark` as task `design`; `cold-tier-step2` takes `design/cold-tier` as task `design` (337 commits behind strata main).
4. `wbuf-inline`: its review saw neither strata:8b1de41 nor strata:c439e69; the new review is scoped to strata:7791520..9ce248e; the branch is 84 commits behind main; "CI green at strata:9ce248e" is the lead's word, not checked here.
5. Perf: "the keep-alive watch byte's extra read" has no ledger; `perf-watch-peek` reads it as http's `CancelWatch` peek. Is prospect's landed sliced residual (1.010 to 1.028) enough for T12?
6. `postings-fuzz-memory` has no ledger or numbers; the target is assumed to be prospect's `fuzz_postings`.
7. The barrier warning (D10, D16) is split into completion, strata and Xapiand quests ordered by `--after`.
8. `name-index`: does it gate the release? road-to-release records the limit but schedules no fix.
9. `manifest-log` is not on road-to-release; it is live strata work under D25. Keep it in this campaign?
10. `closing-benchmark` is blocked: its design and proof use AWS (metal window Q7), the 2026-11-04 rule forbids AWS.
11. `sortable-encoding` blocks on the owner's Q1-Q3; its step 2 (no byte change) may land early; `design/sortable-codec` (REJECTed, superseded on Q1 (a)) is not adopted.
12. `license-closure` adopts `fix/license-actions` for its one unlanded commit (Xapiand:e63910117, the generated sample image); the owner's actions are `[decision]` criteria.
13. `direct-pins` may first need repins in the libraries whose pins disagree (master still has "declared first" blocks).
14. `x7-gate --needs x-pool,perf-watch-peek`; `sortable-encoding --after x7-gate`; `release --after` every Phase 8 quest: the lead's ordering.
15. The architect gate review of X7 becomes the correctness and security lenses; the X3-1 "light architect check" happened (strata `docs/reviews/x3-1-chunked-registry.md`).
16. The queued baseline that completed after 2026-11-05 ("Write that full local baseline"): its output and the rows it serves are not in either repository; name them (x7-gate A5, A11, A12 or closing-benchmark A5).
17. Detent's M1 (off http's offload path) is recorded as D27 on its lead's report, not checked in Detent's repository.
18. Checks: Xapiand sanitizers follow `docs/dev/testing.md` (Homebrew `llvm@22`); library sanitizers use `/usr/local/opt/llvm`; Linux runs (D24, the local VM) are not encoded as checks: add `--later` checks once the VM command is fixed.
19. `map.sh` and `history.sh` need `../prospect`, `../http`, `../completion`, `../sortable` beside the home (only `../strata` was named).
20. `history.sh` makes about three commits per quest (282) as the steps say; `ONE_COMMIT=1` makes one (rehearsed: same map).
21. The seven agent-decided behavior changes the audit flags are reserved for the owner; no quest carries them.
22. Migration README step 7 (a `Status: HISTORY` line on the old ledgers, AGENTS.md pointing at `.campaign/xapiand/`) is not in these files.
23. gcc-hang (`fix/gcc-hang`, Xapiand:3dff9710c, unmerged): superseded by task:d080786 (http-wave: g++-13 ctest 35 of 35)? Not placed.
24. `queryparser.md` (strata): the work landed in Kronuz/queryparser (not cloned); step 3b awaits review on `step3b`. History quest there, or a live quest?
25. `automata.md` (phase5/automata) and `bisection-pool.md` (in review since 2026-10-17): live code not on main and on no plan. Park or plan?
26. Library work of 7c in other repositories (task L1, threadpool L2, http and reactor L3, prospect L4, storage L6, reactor and http S1) has its ledgers there; only strata's L5 has a history quest.

## History quests: judgment calls

1. Risk is the work's own: `gate7a-l1` and `gate7a-fixes` critical (removed user-visible search parameters and features), `seam-census` and `x1-delete-xapian` critical (reindex, cluster removed); `format13`, `f14-step1`, `strata-pins-2026-11`, `retention`, `key-field` high, not critical (pre-freeze formats, no user data migrated).
2. A criterion its ledger left unticked, partial, failed or deferred has an `open:` Evidence row in its journal, so the map scores it unmet: Appendix C.
3. Landing commits that are not a merge of the work: `xapiand-on-strata` and `behavior-changes-audit` at Xapiand:4b37d3a67 (the ledgers brought home; the design merged into strata at strata:28889513); `research-indexing`, `no-stalls`, `lanebuf`, `branch-triage` at strata:18febff7 (their documents landed by the triage; prototypes never merged); `format13` at strata:76fa331 (the checkpoint after the merge ff5392e4); `layers-refactor` at strata:3e3f8b9a and `core-lifecycle-tests` at strata:8713d2b7 (last commit of the work on main's first-parent line); `cold-tier-step2-item5` at strata:676285d (main's head when `phase/manifest-log-steps12` merged it at strata:591658a6; off main's first-parent line).
4. Merged: `seam-write.md` (27 lines) into `seam-census`; the two "Item 5b" sections of `cold-tier-step2.md` into `cold-tier-step2-item5b` (one landing, strata:5d742b24); engine-query's two steps and engine-search's X4a-X4c into one quest each (one ledger each).
5. Not given history quests: strata ledgers marked HISTORY whose code never reached main (`indexing-gap`, `query-counters`, `query-exp`, `query-gap`, `reorder-speed`, `scale-threads`, `small-indexes`), the parked and rejected ones (`index-size`, `encode-once`), and design ledgers whose steps have their own quests (`shared-reader.md`, K1-K8 unticked; `cold-tier.md`).
6. History journals cite reviews by path and verdict as their ledgers name them; some reviews live in another repository (strata `docs/reviews/phase-7a-gate.md` for Xapiand's gate 7a quests, prospect `docs/perf/format13-landing.md`); none is accepted as a campaign review.

Appendix A lists the history quests; Appendix B accounts for every ledger; Appendix C lists the history quests with open criteria.

## Appendix A: history quests

| History quest | Landed at | Date | Source ledger | Milestone |
|---|---|---|---|---|
| `engine-core` | Xapiand:566943e9e | 2026-10-21 | Xapiand `docs/status/engine-core.md` | Phase 0.5 |
| `pool-concurrency` | Xapiand:2466dac5c | 2026-10-26 | Xapiand `docs/status/pool-concurrency.md` | Phase 0.5 |
| `offload-bridge` | Xapiand:44f628903 | 2026-10-26 | Xapiand `docs/status/offload-bridge.md` | Phase 0.5 |
| `offload-followups` | Xapiand:201414347 | 2026-10-26 | Xapiand `docs/status/offload-followups.md` | Phase 0.5 |
| `prometheus-overflow` | Xapiand:55316114d | 2026-10-26 | Xapiand `docs/status/prometheus-overflow.md` | Phase 0.5 |
| `engine-query` | Xapiand:1ab0ce077 | 2026-10-26 | Xapiand `docs/status/engine-query.md` | Phase 0.5 |
| `engine-search` | Xapiand:cca0011bb | 2026-10-27 | Xapiand `docs/status/engine-search.md` | Phase 0.5 |
| `census` | Xapiand:829a67308 | 2026-10-27 | Xapiand `docs/status/census.md` | Phase 0.5 |
| `seam-census` | Xapiand:d0b43fad2 | 2026-10-27 | Xapiand `docs/status/seam-census.md` | Phase 0.5 |
| `cr1-fix` | Xapiand:6c5c1a40d | 2026-10-27 | Xapiand `docs/status/cr1-fix.md` | Phase 0.5 |
| `numeric-range` | Xapiand:5f356d78f | 2026-10-27 | Xapiand `docs/status/numeric-range.md` | Phase 0.5 |
| `alpha10-builds` | Xapiand:dee2deaad | 2026-10-28 | Xapiand `docs/status/alpha10-builds.md` | Xapian-only release |
| `strata-engine-write` | Xapiand:5d7f3263d | 2026-10-28 | Xapiand `docs/status/strata-engine-write.md` | Phase 7a |
| `strata-engine-search` | Xapiand:4caf46c9b | 2026-10-28 | Xapiand `docs/status/strata-engine-search.md` | Phase 7a |
| `strata-engine-callbacks` | Xapiand:5c8b6651d | 2026-10-28 | Xapiand `docs/status/strata-engine-callbacks.md` | Phase 7a |
| `strata-engine-text` | Xapiand:7c5dca498 | 2026-10-28 | Xapiand `docs/status/strata-engine-text.md` | Phase 7a |
| `seam-fields` | Xapiand:8e1183c1a | 2026-10-28 | Xapiand `docs/status/seam-fields.md` | Phase 7a |
| `strata-fields` | Xapiand:9fa890ecc | 2026-10-28 | Xapiand `docs/status/strata-fields.md` | Phase 7a |
| `gate7a-fixes` | Xapiand:8fb9032a2 | 2026-10-28 | Xapiand `docs/status/gate7a-fixes.md` | Phase 7a |
| `phrase` | Xapiand:fade84b1e | 2026-10-29 | Xapiand `docs/status/phrase.md` | Phase 7a |
| `t11-barriers` | Xapiand:7125dfc28 | 2026-10-29 | Xapiand `docs/status/t11-barriers.md` | Phase 7a |
| `gate7a-l1` | Xapiand:10e0b0aaf | 2026-10-29 | Xapiand `docs/status/gate7a-l1.md` | Phase 7a |
| `gate7a-l2` | Xapiand:addacf386 | 2026-10-29 | Xapiand `docs/status/gate7a-l2.md` | Phase 7a |
| `xapiand-on-strata` | Xapiand:4b37d3a67 | 2026-11-03 | Xapiand `docs/status/xapiand-on-strata.md` | Phase 7a |
| `phase7c-design` | Xapiand:05c8dc8ff | 2026-10-29 | Xapiand `docs/status/phase7c.md` | Phase 7c, landed steps |
| `phase7c-s1` | Xapiand:f3bd508f8 | 2026-10-29 | Xapiand `docs/status/phase7c.md` | Phase 7c, landed steps |
| `x1-delete-xapian` | Xapiand:0ae3505c9 | 2026-10-29 | Xapiand `docs/status/x1-delete-xapian.md` | Phase 7c, landed steps |
| `x1-tests` | Xapiand:0ae3505c9 | 2026-10-29 | Xapiand `docs/status/x1-tests.md` | Phase 7c, landed steps |
| `phase7c-x2` | Xapiand:d25fb70a4 | 2026-10-30 | Xapiand `docs/status/phase7c-x2.md` | Phase 7c, landed steps |
| `phase7c-x2b` | Xapiand:d25fb70a4 | 2026-10-30 | Xapiand `docs/status/phase7c-x2b.md` | Phase 7c, landed steps |
| `phase7c-x3` | Xapiand:d5a2e746c | 2026-11-01 | Xapiand `docs/status/phase7c-x3.md` | Phase 7c, landed steps |
| `phase7c-x4` | Xapiand:10ab134ad | 2026-11-02 | Xapiand `docs/status/phase7c-x4.md` | Phase 7c, landed steps |
| `phase7c-x5` | Xapiand:d3f6cc1fd | 2026-11-02 | Xapiand `docs/status/phase7c-x5.md` | Phase 7c, landed steps |
| `phase7c-x6` | Xapiand:2ff594ebe | 2026-11-02 | Xapiand `docs/status/phase7c-x6.md` | Phase 7c, landed steps |
| `phase7c-x4-r1` | Xapiand:8ef0f8633 | 2026-11-02 | Xapiand `docs/status/phase7c-x4-r1.md` | Phase 7c, landed steps |
| `pins-2026-11` | Xapiand:81c311799 | 2026-11-02 | Xapiand `docs/status/pins-2026-11.md` | Phase 7c, landed steps |
| `phase7c-gate-prep` | Xapiand:11e3d82a4 | 2026-11-02 | Xapiand `docs/status/phase7c-gate-prep.md` | Phase 7c, landed steps |
| `phase7c-linux-gate-fixes` | Xapiand:ebfb8fdfe | 2026-11-02 | Xapiand `docs/status/phase7c-linux-gate-fixes.md` | Phase 7c, landed steps |
| `x-pool-step1` | Xapiand:4f873c0e2 | 2026-11-02 | Xapiand `docs/status/phase7c-x-pool.md` | Phase 7c, landed steps |
| `phase7c-task-wave` | Xapiand:b6b8f7534 | 2026-11-03 | Xapiand `docs/status/phase7c-task-wave.md` | Phase 7c, landed steps |
| `linux-sanitizers` | Xapiand:4bb979da8 | 2026-11-03 | Xapiand `docs/status/linux-sanitizers.md` | Phase 7c, landed steps |
| `phase7c-wide-search` | Xapiand:4f1cc9a94 | 2026-11-03 | Xapiand `docs/status/phase7c-wide-search.md` | Phase 7c, landed steps |
| `phase7c-http-wave` | Xapiand:0f01d54dc | 2026-11-03 | Xapiand `docs/status/phase7c-http-wave.md` | Phase 7c, landed steps |
| `phase7c-wide-sr8` | Xapiand:1dc98c213 | 2026-11-03 | Xapiand `docs/status/phase7c-wide-sr8.md` | Phase 7c, landed steps |
| `license-audit` | Xapiand:5caed332b | 2026-11-03 | Xapiand `docs/status/license-audit.md` | Phase 8 groundwork |
| `behavior-changes-audit` | Xapiand:4b37d3a67 | 2026-11-03 | Xapiand `docs/status/behavior-changes-audit.md` | Phase 8 groundwork |
| `rocksdb-spec` | strata:d4fec9a | 2026-10-20 | strata `docs/status/rocksdb-spec.md` | strata Phase 4 |
| `core-vs-rocksdb` | strata:b8a2cb6 | 2026-10-20 | strata `docs/status/core-vs-rocksdb.md` | strata Phase 4 |
| `core-split` | strata:3f88a15 | 2026-10-20 | strata `docs/status/core-split.md` | strata Phase 4 |
| `repo-split` | strata:426cc59 | 2026-10-20 | strata `docs/status/repo-split.md` | strata Phase 4 |
| `format-14-kv` | strata:4d36aba | 2026-10-20 | strata `docs/status/format-14-kv.md` | strata Phase 4 |
| `merge-wait` | strata:6e13416 | 2026-10-20 | strata `docs/status/merge-wait.md` | strata Phase 4 |
| `f14-step1` | strata:75d1e7e | 2026-10-20 | strata `docs/status/f14-step1.md` | strata Phase 4 |
| `format13` | strata:76fa331 | 2026-10-20 | strata `docs/status/format13.md` | strata Phase 4 |
| `layers-refactor` | strata:3e3f8b9 | 2026-10-18 | strata `docs/status/layers-refactor.md` | strata Phase 4 |
| `core-lifecycle-tests` | strata:8713d2b | 2026-10-20 | strata `docs/status/core-lifecycle-tests.md` | strata Phase 4 |
| `research-indexing` | strata:18febff | 2026-10-30 | strata `docs/status/research-indexing.md` | strata Phase 4 |
| `no-stalls` | strata:18febff | 2026-10-30 | strata `docs/status/no-stalls.md` | strata Phase 4 |
| `pressure-flush` | strata:5737867 | 2026-10-21 | strata `docs/status/pressure-flush.md` | strata Phase 4 |
| `flush-memory` | strata:5685428 | 2026-10-21 | strata `docs/status/flush-memory.md` | strata Phase 4 |
| `retention` | strata:0156b57 | 2026-10-21 | strata `docs/status/retention.md` | strata Phase 4 |
| `kv-scan` | strata:89f9fd7 | 2026-10-21 | strata `docs/status/kv-scan.md` | strata Phase 4 |
| `compact-slots` | strata:8fcd4e3 | 2026-10-21 | strata `docs/status/compact-slots.md` | strata Phase 4 |
| `shared-reader-core` | strata:c576997 | 2026-10-21 | strata `docs/status/shared-reader-core.md` | strata Phase 4 |
| `reclaim-backlog` | strata:b3dcc48 | 2026-10-21 | strata `docs/status/reclaim-backlog.md` | strata Phase 4 |
| `merge-stream` | strata:6589f32 | 2026-10-21 | strata `docs/status/merge-stream.md` | strata Phase 4 |
| `shared-reader-views` | strata:23e78bb | 2026-10-26 | strata `docs/status/shared-reader-views.md` | strata write buffer |
| `flush-memory-bound` | strata:fb583d8 | 2026-10-26 | strata `docs/status/flush-memory-bound.md` | strata write buffer |
| `install-own-files` | strata:ef48ed0 | 2026-10-26 | strata `docs/status/install-own-files.md` | strata write buffer |
| `stored-short-section` | strata:75716f0 | 2026-10-26 | strata `docs/status/stored-short-section.md` | strata write buffer |
| `write-buffer` | strata:39c9cc1 | 2026-10-26 | strata `docs/status/write-buffer.md` | strata write buffer |
| `write-buffer-reads` | strata:2f56d64 | 2026-10-26 | strata `docs/status/write-buffer-reads.md` | strata write buffer |
| `reclaim-bg` | strata:45e6b32 | 2026-10-26 | strata `docs/status/reclaim-bg.md` | strata write buffer |
| `flushmem` | strata:965339c | 2026-10-26 | strata `docs/status/flushmem.md` | strata write buffer |
| `idle-index` | strata:1a04bc3 | 2026-10-27 | strata `docs/status/idle-index.md` | strata write buffer |
| `lanebuf` | strata:18febff | 2026-10-30 | strata `docs/status/lanebuf.md` | strata write buffer |
| `fields-and-keys` | strata:0078bb6 | 2026-10-27 | strata `docs/status/fields-and-keys.md` | strata fields and keys |
| `field-table` | strata:c4ea4ce | 2026-10-28 | strata `docs/status/field-table.md` | strata fields and keys |
| `key-field` | strata:cef4e59 | 2026-10-28 | strata `docs/status/key-field.md` | strata fields and keys |
| `phase7c-l5` | strata:c82ac9f | 2026-10-29 | strata `docs/status/phase7c-l5.md` | strata Phase 7c steps |
| `branch-triage` | strata:18febff | 2026-10-30 | strata `docs/status/branch-triage-2026-10-30.md` | strata Phase 7c steps |
| `strata-pins-2026-11` | strata:f2d7626 | 2026-11-02 | strata `docs/status/pins-2026-11.md` | strata Phase 7c steps |
| `drop-threadpool` | strata:864ea69 | 2026-11-02 | strata `docs/status/drop-threadpool.md` | strata Phase 7c steps |
| `x3-1-chunked-registry` | strata:80e8f5a | 2026-11-02 | strata `docs/status/x3-1-chunked-registry.md` | strata Phase 7c steps |
| `cold-tier-step1` | strata:66ddb16 | 2026-10-27 | strata `docs/status/cold-tier-step1.md` | strata cold tier, landed items |
| `cold-tier-item5` | strata:081b7e7 | 2026-11-02 | strata `docs/status/cold-tier-item5.md` | strata cold tier, landed items |
| `cold-tier-step2-item1` | strata:8d7467b | 2026-11-02 | strata `docs/status/cold-tier-step2.md` | strata cold tier, landed items |
| `cold-tier-step2-item2` | strata:033e546 | 2026-11-02 | strata `docs/status/cold-tier-step2.md` | strata cold tier, landed items |
| `cold-tier-step2-item3` | strata:be5c5af | 2026-11-02 | strata `docs/status/cold-tier-step2.md` | strata cold tier, landed items |
| `cold-tier-step2-item4` | strata:4795b7e | 2026-11-03 | strata `docs/status/cold-tier-step2.md` | strata cold tier, landed items |
| `cold-tier-step2-item5` | strata:676285d | 2026-11-04 | strata `docs/status/cold-tier-step2.md` | strata cold tier, landed items |
| `cold-tier-step2-item6` | strata:8bb648d | 2026-11-03 | strata `docs/status/cold-tier-step2.md` | strata cold tier, landed items |
| `cold-tier-step2-item5b` | strata:5d742b2 | 2026-11-04 | strata `docs/status/cold-tier-step2.md` | strata cold tier, landed items |
| `manifest-log-steps12` | strata:591658a | 2026-11-04 | strata `docs/status/manifest-log-steps12.md` | strata manifest log steps 1-2 |

## Appendix B: every ledger, and where it went

Xapiand `docs/status/` on master (47):

- History quest of the same name (43): `alpha10-builds`, `behavior-changes-audit`, `census`, `cr1-fix`, `engine-core`, `engine-query`, `engine-search`, `gate7a-fixes`, `gate7a-l1`, `gate7a-l2`, `license-audit` (its open actions: `license-closure`), `linux-sanitizers`, `numeric-range`, `offload-bridge`, `offload-followups`, `phase7c-gate-prep` (its follow-ups: `gate-prep-followups`), `phase7c-http-wave`, `phase7c-linux-gate-fixes`, `phase7c-task-wave`, `phase7c-wide-search` (its gate row: `x7-gate` A16), `phase7c-wide-sr8`, `phase7c-x2`, `phase7c-x2b`, `phase7c-x3`, `phase7c-x4`, `phase7c-x4-r1` (its follow-up: `gate-prep-followups` A7), `phase7c-x5`, `phase7c-x6`, `phrase`, `pins-2026-11`, `pool-concurrency`, `prometheus-overflow`, `seam-census`, `seam-fields`, `strata-engine-callbacks`, `strata-engine-search`, `strata-engine-text`, `strata-engine-write`, `strata-fields`, `t11-barriers`, `x1-delete-xapian`, `x1-tests`, `xapiand-on-strata` (its rulings: PLAN Decisions).
- `phase7c.md`: history `phase7c-design` (its first part) and `phase7c-s1` (the S1 section).
- `phase7c-x-pool.md`: history `x-pool-step1` (master's step-1 ledger) and quest `x-pool` (the step-3 ledger on its branch).
- `seam-write.md`: history `seam-census`.
- `road-to-release.md`: PLAN.md (superseded by it).

Xapiand ledgers only on branches: `closing-benchmark.md`, `closing-bench-harness.md` -> `closing-benchmark`; `sortable-encoding.md` -> `sortable-encoding`; `sortable-codec.md` -> not adopted (superseded on Q1 (a)); `gcc-hang.md` -> not placed (question 23); `failover.md`, `pki.md`, `tls-gate.md` -> PLAN Non-goals (parked distribution work).

strata `docs/status/` on main (62):

- History quest of the same name (41): `branch-triage-2026-10-30` (as `branch-triage`), `cold-tier-item5`, `cold-tier-step1`, `compact-slots`, `core-lifecycle-tests`, `core-split`, `core-vs-rocksdb`, `drop-threadpool`, `f14-step1`, `field-table`, `fields-and-keys`, `flush-memory`, `flush-memory-bound`, `flushmem`, `format-14-kv`, `format13`, `idle-index`, `install-own-files`, `key-field`, `kv-scan`, `lanebuf`, `layers-refactor`, `manifest-log-steps12` (step 3: `manifest-log`), `merge-stream`, `merge-wait`, `no-stalls`, `phase7c-l5`, `pins-2026-11` (as `strata-pins-2026-11`), `pressure-flush`, `reclaim-backlog`, `reclaim-bg`, `repo-split`, `research-indexing`, `retention`, `rocksdb-spec`, `shared-reader-core`, `shared-reader-views`, `stored-short-section`, `write-buffer`, `write-buffer-reads`, `x3-1-chunked-registry`.
- `cold-tier-step2.md`: history `cold-tier-step2-item1` to `-item6` and `cold-tier-step2-item5b`; what remains: quest `cold-tier-step2`.
- `cold-tier.md` (the design ledger): quest `cold-tier-step2` (S9, the T rows, step 1's K3, K4, K6-K8).
- `hybrid-search.md`: quest `hybrid-go-no-go`; `landscape.md`: its context (the AI steer conditional on fit), no quest.
- `README.md` (the earlier index): PLAN.md.
- `shared-reader.md` (design, revision 3; K1-K8 unticked): covered by history `shared-reader-core`, `shared-reader-views` and `x3-1-chunked-registry` (revision 4); lead confirms.
- `durability-modes.md`, `peer-transport.md`, `simulator.md`: PLAN Non-goals (parked by the owner, D4).
- Not placed: `automata.md`, `bisection-pool.md`, `queryparser.md` (questions 24, 25); `encode-once.md` (REJECTED); `index-size.md` (PARKED; its reclaim landed as `reclaim-bg`); `indexing-gap.md`, `query-counters.md`, `query-exp.md`, `query-gap.md`, `reorder-speed.md`, `scale-threads.md`, `small-indexes.md` (HISTORY, code never on main).

strata ledgers only on branches: `wbuf-inline.md` -> `wbuf-inline`; `manifest-log.md` -> `manifest-log`; `f13-gate.md`, `f13-preflight.md`, `f13-query.md`, `inverter.md`, `inverter-followups.md` -> moved to prospect by the repo split (prospect's own `docs/status/`), not mapped; `library-issues-2026-11.md` -> moved to cpp-libraries (strata:557d97a0), not mapped; `road-to-release.md`, `xapiand-on-strata.md` -> moved to Xapiand (above).

## Appendix C: history quests with open criteria

Each history criterion was compared with its ledger's ticks. Where the ledger left the box unticked,
marked it partial (`[~]`), or reports it failed, not run or deferred, the journal's Evidence row reads
`open: <why> in <ledger>` and the Result's last line names it ("Left open: ..."); every other row
cites its ledger, which exists at Xapiand master or strata main (`phase7c-task-wave` A12 cites
`docs/status/phase7c-http-wave.md`, the ledger that ticks those items). 30 criteria in 17 quests are
open; 516 are met.

Rehearsed again on fresh scratch clones with campaign at agent-campaign:f34f718 (it scores a landing
recorded with `--earlier` from its journal's Evidence): `campaign check` ok; `campaign map` reports 94
of 115 quests landed, 516 of 716 criteria met, and every history quest's unmet criteria equal the
list below.

| History quest | Ledger | Open criteria and why |
|---|---|---|
| `cold-tier-item5` | `docs/status/cold-tier-item5.md` | A8: carried to step 2's sweep, not run here |
| `cold-tier-step1` | `docs/status/cold-tier-step1.md` | A5: marked partial ([~]); CI on every platform left to the lead |
| `cold-tier-step2-item5b` | `docs/status/cold-tier-step2.md` | A9: revision 8 text written in the ledger, not applied to the design |
| `core-split` | `docs/status/core-split.md` | A8: CR4 open until the final tip is green on all 8 jobs |
| `format13` | `docs/status/format13.md` | A1-A5, A7, A10, A12, A14: left unticked in the ledger (K1-K5, K7, K10, K12, K14) |
| `key-field` | `docs/status/key-field.md` | A6: left unticked; CI pushed, not awaited |
| `manifest-log-steps12` | `docs/status/manifest-log-steps12.md` | A8: left unticked in the ledger (TSan; nothing concurrent) |
| `phase7c-task-wave` | `docs/status/phase7c-task-wave.md` | A1: left unticked, deferred to the next half (the http wave); A6: left unticked, deferred to the next half (F2, the http wave); A8: left unticked, deferred to the next half (H1 removes the caller); A9: left unticked, deferred to the next half (reactor R) |
| `phase7c-x2` | `docs/status/phase7c-x2.md` | A5: marked partial ([~]); handler and schema waits deferred to X2b |
| `repo-split` | `docs/status/repo-split.md` | A5: both repositories' CI runs in progress when recorded |
| `shared-reader-views` | `docs/status/shared-reader-views.md` | A5: left unticked; the last push's CI not run |
| `strata-engine-search` | `docs/status/strata-engine-search.md` | A8: partial: the server under ASan and TSan (T10) not run |
| `strata-engine-text` | `docs/status/strata-engine-text.md` | A2: CJK (`cjk_words`) query compatibility left open |
| `strata-engine-write` | `docs/status/strata-engine-write.md` | A8: partial: D8's saved `IndexSettings` field not done |
| `strata-fields` | `docs/status/strata-fields.md` | A6: marked partial ([~]): strata E2E 507 of 576, 69 refusals; A9: marked partial ([~]): not at merge, no test; A10: marked partial ([~]): CR2's query-side marker awaits the analysis change |
| `write-buffer` | `docs/status/write-buffer.md` | A7: left unticked; CI on the ledger-only head not awaited |
| `write-buffer-reads` | `docs/status/write-buffer-reads.md` | A11: left unticked in the ledger (CI green on the branch) |

Rulings in this pass: a ticked box counts as met even when its text names a leftover
(`phase7c-task-wave` A3, A7, A10; `phase7c-x3` A4, M31 not run; `x3-1-chunked-registry` A8, Linux
and FreeBSD not run); a criterion whose own text states something open is open.

## The audit (2026-11-08)

Auditors checked the migration against everything on the laptop; their findings are in
`migration-audit-2026-11-08/` (`xapiand.md`, `xapiand-roadmap.md`, `libraries.md`). What they changed:

1. The roadmap: 21 milestones from Phase 0 to the recovery tooling, in strata `docs/PLAN.md`'s
   order; 8 history quests for the phases and gates that finished without a ledger (Phases 0, 1 and
   2, Phase 3's lifecycle rows, Track C's C1, Phase 0.5's X1, gates 0.5 and 7a); 11 planned or
   blocked quests for the future and parked phases; 73 quests moved out of the ledger-era
   milestones; decisions D28 to D34.
2. The library ledgers this campaign owns: 41 history quests; `library-issues` planned.
3. State the lead never recorded: X-POOL's docs E2E failure, wbuf-inline's FreeBSD failure (A11),
   the Linux 7c rows and the closing baseline (now in `docs/perf/`), and the final Xapian-only
   release (`xapiand-final-release`, D34).
4. Fidelity: 14 history journals name their own code review; `t11-barriers` is current;
   `phase7c-http-wave` A11 is restored; `barrier-warning-strata` A5 and A6 carry the commit-barriers
   review's C2 and C3.
5. Safety: worktrees' uncommitted work, heads on no branch and stashes are on `wip/laptop/*`
   branches; the tag `x2-wip-reference` is pushed; `strata-oracle` is on GitHub (private); the
   strata branches the owner had deleted on 2026-11-02 are deleted again. The branches this
   report's appendix omits are listed in `migration-audit-2026-11-08/xapiand.md`, G11.
