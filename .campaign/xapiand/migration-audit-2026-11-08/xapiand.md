# Audit of the Xapiand and strata migration: checks 1 to 4

Auditor `xapiand`, 2026-11-08. Laptop session `remote-c5c46609-xapiand-97695e` (read-only; scratch in
`~/.scratch/audit-xapiand/`). Sources: the laptop's two homes, 156 worktrees, `~/.scratch`, the job store,
`~/.claude/projects/-Users-kronuz-code-Kronuz-Xapiand/` (the lead's transcript tail, its last 12 subagents,
its 59 memory notes); GitHub through the local clones; the migrated campaign at Xapiand:6f7740203 (=
`wip/master-migrated-2026-11-08`, = laptop master) and strata:8e206304 (= laptop main). Check 0 (roadmap, PLAN
milestones) belongs to `xapiand-roadmap`; notes for it are at the end.

## Verdict

The migration is complete at the level of files and refs: all 109 ledgers on main and the 18 found only on
branches are accounted for, all 209 local branches are on GitHub at the same commit or an ancestor, the 94
history quests cite paths that exist (545 of 546 evidence rows at their landing commits; 190 of 191 Result
paths on main, 17 of them brought home after the landing by the 2026-10-30 triage), and the open criteria of
the checkbox ledgers match their unticked boxes exactly. What it missed is the lead's last 36 hours, which no
ledger recorded: the adopted X-POOL head fails the docs E2E (plain and TSan), wbuf-inline's "green" CI failed
FreeBSD's `write_buffer` on its first attempt, a Linux T12 run shows RESTORE at 0.18x pre-7c, and the closing
baseline's numbers (one REGRESSION against the Xapian-only release) exist only in `~/.scratch`. It also missed
one owner-ruled plan item (the final Xapian-only release, three branches of work), two strata notes that never
reached main, two open review conditions, one dropped criterion, and the review file of 14 history quests. 18
files of uncommitted worktree content are on no branch. Every gap below has a prepared fix except where it
says "note".

## Coverage

| Source item | Where it went | Status |
|---|---|---|
| Xapiand `docs/status/` on master, 47 | 43 same-name history quests; `phase7c.md` split in two; `phase7c-x-pool.md` history + live `x-pool`; `seam-write.md` in `seam-census`; `road-to-release.md` in PLAN | OK |
| strata `docs/status/` on main, 62 | 41 history quests; `cold-tier-step2.md` 7 history + live; `cold-tier.md`, `hybrid-search.md` live; 3 parked to Non-goals; 10 HISTORY ledgers with no code on main and 2 rejected/parked listed | OK; `automata`, `bisection-pool` to roadmap; `queryparser.md` GAP G10 |
| Ledgers only on branches (every GitHub branch scanned): Xapiand 8, strata 10 (5 moved to prospect, 1 to cpp-libraries, 2 to Xapiand) | live quests or listed in the report | OK (G19 for `gcc-hang`) |
| Ledgers marked `Status: HISTORY` | Xapiand 47 of 47; strata 52 of 62 by the migration, the other 10 already HISTORY | OK |
| Xapiand `docs/reviews/`, 28 | 20 cited by journals; 7 belong to history quests but are not cited; `transport-security-design` parked | GAP G14 (fixed) |
| strata `docs/reviews/`, 65 | 38 cited; 7 belong to history quests, uncited; 17 phase/parked (roadmap); `dir-lock-code`, `query-counters-code`, `queryparser-step1` unplaced | GAP G14 (fixed); rest to roadmap |
| Design docs, Xapiand 6 / strata 30 | all cited except Xapiand `transport-security` (parked) and strata `frozen-index`, `phase-2`, `placement-strategies`, `product-landscape`, `rocksdb-survey`, `track-c`, `workload-profiles` | roadmap |
| Handoffs | none in either repository; July files in `~/code/Kronuz` predate the campaign | OK |
| Plan items of road-to-release | report's step mapping | OK; the final Xapian-only release is no ledger's: G7 |
| Open decisions | PLAN open questions 1-8 | G12 (Q2 decided, Q8 answered, Q3 the lead's) |
| Unmerged branches, Xapiand 27 | 6 adopted, 2 in the report, 3 parked by ledger, 10 with every commit patch-equivalent on master, 6 unlisted with unique commits | GAP G7, G11 |
| Unmerged branches, strata 43 | 4 adopted, 35 in `branch-triage-2026-10-30.md` (history quest), 2 with no unique commit, 2 unlisted | GAP G8 |
| Local branches on GitHub (209; `git for-each-ref` on the laptop against `git ls-remote` here) | 101 at the same name and sha; 108 (merged branches deleted on GitHub, 3 behind their `wip/laptop/*` copies, the two migrated mains) reachable from a GitHub branch | OK |
| Local tags | `x2-wip-reference` laptop-only; 89 v0.x tags differ (pre-campaign) | GAP G6; note |
| Worktrees with uncommitted changes: Xapiand 3 of 60, strata 32 of 96 | 18 files in no commit; the rest equal main, in history, deleted fixtures or build output | GAP G5 |
| Campaign job store | 111 job directories: 101 Detent's, 7 with no campaign, 3 without a spec; 0 Xapiand's | OK |
| `.claude/worktrees` | none in either repository | OK |
| Results outside git | 4 sets cited by nothing committed; ~760 `.scratch` citations whose numbers the citing docs carry (sampled) | GAP G1-G4, G20 |
| The lead's last state (transcript to 2026-11-05 01:30Z, 12 subagents, memory) | 4 unrecorded states, 1 unrecorded ruling, 6 PLAN-level decisions | G1, G2, G3, G7, G12 |

## Gaps, by severity

### Lost work or lost evidence

**G1. The adopted X-POOL head fails the docs E2E; only `.scratch` knows.** The lead's "Finish X-POOL on strata
main" agent stopped at the quota (2026-11-05 01:27Z) while "waiting for the plain checks, mutants and E2E
reruns". Its results at Xapiand:0638e61fd, in `~/code/Kronuz/Xapiand-wt/x-pool-step3/.scratch/`: plain E2E NOT
GREEN, 54 failing assertions (`plain-e2e.log`: "Load sample data" RESTORE gives no response, `doc_count` 679
of 1,000); TSan E2E NOT GREEN, 48 (`tsan-e2e.log`); ASan E2E GREEN; mutants t4-t7 killed; TSan
`blocking_guard` 14 checks, graceful shutdown, wide check pass. The quest says only "last four commits
unverified". Fix: `quests/x-pool.md` Decisions names this evidence (A17 starts from the failure).

**G2. wbuf-inline is not "CI green, ready to land".** Run 41944174997 at strata:9ce248e failed on attempt 1 in
`ci / freebsd-x86_64`: `write_buffer` (`test/write_buffer_test.cc:506` `merges_done`, `:516` `fenced`), the
test the branch changes; it passed on attempt 2. The lead was reading that failure ("not a known flake") when
the limit hit (transcript 2026-11-05T01:22Z). The report (question 4) and the quest say green. Fix:
`quests/wbuf-inline.md` gains A11 (explain and fix, or show on main; 300 repeats on the FreeBSD VM) and a
corrected Decisions line. A criterion added, none weakened.

**G3. Linux 7c gate rows of 2026-11-03, recorded nowhere.** AWS `c7a.8xlarge`, Xapiand:432299ed0 against
pre-7c Xapiand:142b2ca77, both on strata: every functional row passes except T10 Lua under ASan (a light GET
134.5 ms, limit 50); T12 loadtest RESTORE 1,727-1,836 against 10,088-10,427 documents/s (0.18x; x7-gate A12
wants 1.0x), queries 1.22x. Source: `~/.scratch/leases/results/strata-gate7c/`, `~/.scratch/gate7c-res/lt/`.
Fix: `Xapiand/docs/perf/phase7c-linux-2026-11-03/` (README, `rows.tsv`, `t12-loadtest.tsv`, 3 KB) and
`quests/x7-gate.md` Decisions.

**G4. The closing baseline of 2026-11-04 is only in `~/.scratch`.**
`~/.scratch/xapiand-baseline/20261105T033044Z/` (harness Xapiand:ec4e6d937, 200,000 documents, every row, 5
rounds, local VM, every step rc 0) and its driver
`Detent/.scratch/overnight/done/Xapiand-baseline.{sh,log,rc}`. Cited only by `.campaign` files ("Neither
repository records it yet"). It shows S behind E on T4b aggregations (histogram 71.6x, terms 14.2x) and a
REGRESSION against X: T11 idle memory 1.28x. Fix: `docs/perf/closing-baseline-2026-11-04/` (README,
SUMMARY.txt, report.csv, steps.tsv, phases.tsv, anchors.json, host identity, run.sh; 28 KB), on
`quest/xapiand/closing-benchmark`; `quests/closing-benchmark.md` Decisions cites it.

**G5. 18 files of worktree content are in no commit** (migration step 4 "commit every worktree as wip:
parked" skipped detached worktrees). `git -C WT status --porcelain`, each file's blob searched in all history:
- Xapiand `Xapiand-wt/restore-stream`: `harness/restore_large_check.py` (209 lines, a RESTORE past the 16 MiB
  body cap, chunked and with Content-Length, RSS bound). Prepared: `Xapiand/harness/restore_large_check.py`
  (sha256 840afccf…, equal to the laptop's) for a wip branch, unverified.
- Xapiand `Xapiand-wt/review-cr1` (detached cff88d62c): the CR1 crash probe, `src/database/shard.cc` +25 lines
  (`CR1_CRASH` SIGKILL points), behind `docs/reviews/cr1-replay-durability.md`.
- strata, 16 files in 10 worktrees: `cold-tier-step2-item2/.scratch/review-step2/merged` (staged merge,
  `CMakeLists.txt`); `f13-preflight/.scratch/wt-{cnt,ff,p,pos,q,q2}` (format-13 query-gap experiments;
  `wt-cnt` is a query-counters merge left mid-conflict); `f13-query/.scratch/wt-m16memo`, `wt-m23memo`;
  `format13-abmain` (`CMakeLists.txt`). Their conclusions are in committed perf notes.
- Untracked `TODO.md` in Xapiand master (2026-07-10, 168 lines of owner notes and an audit, pre-campaign).
Fix (the lead, on the laptop): in each worktree `git switch -c wip/laptop/<name> && git add -A && git commit
-m "wip: parked" && git push -u origin HEAD` (`wt-cnt`: `git merge --abort` only after saving `git diff
--cached`); never remove these worktrees before.

**G6. Tag `x2-wip-reference` exists only on the laptop** (Xapiand:d2b0863ef, 2026-07-04, "Xapian
1.5.0->2.0.0 reconcile (NON-BUILDING checkpoint)", 1 commit, 55 files, in no branch). Fix: `git push origin
refs/tags/x2-wip-reference`.

### Missing history or plan

**G7. The final Xapian-only release has no quest.** Owner ruling 2026-10-28 (lead memory
`xapiand-final-and-elysian.md`): the last Xapiand release is cut from the `xapiand` branch with backports
(queryparser for the nested-NEAR heap overflow, numeric id 0, integer edge cases), CI and release workflows
green before the tag, then the repository archived; D12 makes it the closing benchmark's X. Its work sits on
three branches the report never lists: `xapiand` (Xapiand:e692a64b1, 7 commits with no equivalent on master:
aggregation caps, url-parser overflow pin, pins), `fix/xapiand-backports` (Xapiand:ffd1c3708, the queryparser
adapter), `explore/value-compare` (Xapiand:7fcbf0000, id 0 refused). Fix: `quests/xapiand-final-release.md`
(planned, high), then `campaign new xapiand-final-release --title "The final Xapian-only Xapiand release"
--risk high --milestone "Xapian-only release" --files 'src/**,tests/**,docs/**,CMakeLists.txt,cmake/**'
--check 'build-test:light:150=cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON && cmake --build
build -j8 && ctest --test-dir build --output-on-failure'`, `git branch quest/xapiand/xapiand-final-release
xapiand`, `campaign set closing-benchmark after=xapiand-final-release` (keep its other `after`s). The roadmap
auditor's PLAN.md records the ruling as D34 and keeps the "Xapian-only release" milestone for it.

**G8. Two strata explore notes never reached main.** `explore/sync-cpu` (strata:ed0dbeef3, 2026-11-03, PARKED:
commit CPU is file churn, 2.5-10x RocksDB) and `explore/wbuf-bytes` (strata:bec2c0914, 2026-11-04, the
prototype `wbuf-inline` finishes); both postdate the triage, neither is in the report, and their
exploration-log rows are missing on main. Fix: `strata/docs/perf/sync-cpu.md`, `wbuf-bytes.md` (verbatim from
the branches) and `exploration-log.md` (main plus the two rows).

**G9. Two conditions of the commit-barriers review are carried by no quest.** strata
`docs/reviews/commit-barriers-durability.md`: the re-check closed L1, L2, C1 and left C2 (the macOS TSan blind
spot into `calibration.md`, "the lead handles C2") and C3 (faultfs post-mark writes, `t11-barriers.md`
follow-up 3 "Not done"). Fix: `quests/barrier-warning-strata.md` A5, A6.

**G10. `queryparser.md` (strata, L1) is unplaced** (report question 24). Its state is stale: `step3b` is
merged into queryparser main (queryparser:ec56ab3). The repository is in the libraries audit's clones; the
roadmap auditor suggests milestone Phase 7a. Note: one history quest for steps 1-3b at the queryparser merge,
prepared by whoever owns that repository.

**G11. The report's branch appendix omits 8 branches with unique commits**: Xapiand `xapiand`,
`fix/xapiand-backports`, `explore/value-compare` (G7), `explore/agg-blowup` (superseded by the merged
`fix/agg-buckets`), `explore/gate05-index`, `explore/side-findings` (notes on master in `docs/dev/` and
`docs/perf/gate05-index.md`, code never landed), strata `explore/sync-cpu`, `explore/wbuf-bytes` (G8); and
`design/pki`, `fix/failover`, `tls/d1-gate` appear only by ledger name. Also 9 of the 53 strata branches the
owner had deleted on 2026-11-02 are on GitHub again (`explore/{bigdisk,bigmem,bigval,buffer-bytes,f1-ci,
peak-memory,retention,scan,write-p99}`, each at its `archive/` tag), and 43 remain local on the laptop. Fix:
add these lines to `migration-2026-11-08.md`; delete the 9 again (owner-authorized, D33).

**G12. Owner decisions the PLAN misses or asks again** (to `xapiand-roadmap`, which prepares PLAN.md): open
question 2 was decided ("Delete the archived branches.", 2026-11-02T15:43Z) and done at 15:46Z; question 8 is
answered by the 2026-10-20 ruling in strata `docs/PLAN.md` lines 16-30; question 3 is the lead's call per
`phase7c-wide-sr8.md`; question 4's non-AWS option is `~/code/local-benchmarking.md` (2026-11-02, untracked);
unrecorded asks: the architecture book (2026-10-27, strata `docs/book/OUTLINE.md`) and "the catalog design has
to be large-scale-proof" for many nodes (2026-11-03T21:57Z). The roadmap auditor applied all of these (D33 for
the branches, D30 for the formats, its `phase7b` quest for the catalog); the book is
`quests/architecture-book.md` here (planned, blocked on the owner's "later"): `campaign new architecture-book
--repo ../strata --risk low --title "The Architecture and Design book" --files
'docs/book/**,docs/mechanisms/**'` then `campaign set architecture-book status=blocked reason="owner
2026-10-27: later"`.

### Fidelity errors

**G13. `phase7c-http-wave` dropped one of its ledger's 11 ticked criteria** ("The timing checks":
`test_pending_keys_cost_the_loop_nothing` paced, mutant CPsleep). Fix: `quests/phase7c-http-wave.md` A11 and
its journal row.

**G14. 14 history journals miss or misname their own review.** The Result names a design review or "no review"
while the code review is on main: `engine-search` (x4a, x4b), `phase7c-x2` (`phase7c-x2-code`),
`cold-tier-item5` ("review pending"; `cold-tier-item5-code` APPROVE TO LAND), `cold-tier-step1`, `field-table`
and `key-field` ("review remains"; `field-table-check`, `key-field-security` APPROVE), `install-own-files`,
`format13` (`format-13-code`), `x1-delete-xapian`, `strata-engine-callbacks`, `seam-census`
(`seam-write-code`), `f14-step1` (`sr2-sr4`), `offload-bridge` (`engine-offload-architect`), `t11-barriers`
(G15). Fix: the 14 journals, Result lines only.

**G15. `t11-barriers` is stale**: Result and Decisions say "fixes awaiting re-check"; the re-check of
2026-10-29 closed them. Fix: its quest and journal.

**G16. Minor citations.** `manifest-log-steps12`'s Result cites `docs/reviews/manifest-log-design.md`, which
is on `design/manifest-log`, not strata main; `phase7c-task-wave` A12's evidence file postdates its landing
commit (known, report Appendix C). `cold-tier-step2` A14 asks for an edit to a HISTORY ledger. Note.

### Cosmetic

**G17.** strata `AGENTS.md` has no pointer to the campaign (report says both repositories do). Fix:
`strata/AGENTS.md`.

**G18.** strata's query-gap design and review rescore two `gate.py` outputs kept only in `~/.scratch`
(`quick-n2.md`, `n1/gate5.md`, 6 KB each). Fix: `strata/docs/perf/query-gap-gate-output/`.

**G19.** `gcc-hang` (report question 23): `phase7c-http-wave.md` says task:d080786 fixed the g++-13 hang, yet
no g++-13 run of `test_pool` and `test_blocking_guard` is recorded after it. Note: mark it superseded and run
those two tests on g++-13 in x7-gate's Linux jobs.

**G20.** Results outside git that need nothing: the `aws-linux/local-*` VM runs (numbers in the wbuf-inline
and manifest-log ledgers), `leases/results/http-t6-metal` (committed in http `docs/perf/t6-t7-2026-11-03/`),
`cbh2-live` (closing-bench-harness ledger), the pre-7c baseline run 2 (`docs/perf/phase7c-baseline.md`), the
gate 7a and 0.5 raw runs (their gate docs carry the numbers), and review probe directories.

## Unrecorded decisions and plans (check 4)

From the lead's transcript (owner messages 2026-11-02 to 11-05), its last 12 subagents and its memory:
1. Recorded: D17-D27 cover the pin wave, wide search, one shape, nested indexes, the manifest log, no AWS.
2. Unrecorded states: G1 (X-POOL finisher), G2 (FreeBSD failure), G3 (Linux run), G4 (baseline). The
   "Xapiand library repins" agent died on "E2E failed" (2026-11-04T09:30Z); the lead reran it GREEN
   (`~/.scratch/repin-e2e2.log`) before master Xapiand:9ff11c0c6: resolved.
3. Unrecorded ruling: the final Xapian-only release (G7).
4. PLAN-level: G12.
5. Not project work: the Discussions widget fix (done in the blog's Worker), AWS spend handling, skill
   rules (model choice, pipelining), cmux tab cleanup.

## Notes for xapiand-roadmap

- Sent by message and applied in its PLAN.md: G12 (D33, D30, questions 2, 3, 4, 8) and the
  `xapiand-final-release` quest (D34; milestone "Xapian-only release" kept, with `alpha10-builds`).
- Uncited reviews and designs that belong to phases: strata `phase-1`, `phase-2-design`, `phase-3-design`,
  `phase-3-lifecycle`, `stage-1`, `plan-amendments`, `background-merges`, `rocksdb-survey`,
  `track-c-design`, `small-indexes-design`, `query-gap-design`, `dir-lock-code` (Phase 4's dir-lock), the
  parked `durability-modes-*`, `peer-transport-*`, `simulator-design`, `automata-design`; designs
  `frozen-index`, `phase-2`, `placement-strategies`, `product-landscape`, `rocksdb-survey`, `track-c`,
  `workload-profiles`; Xapiand `transport-security` (design and review, the parked HTTPS gate).
- `name-index`, `direct-pins`, the barrier warnings and `postings-fuzz-memory` are Phase 8 "before
  release" items, not phases; the architecture book is a later item with no phase.

## What I did not do

No laptop writes outside `~/.scratch/audit-xapiand/`; no push. I did not rerun any job, so no fix above claims
a test result. History fidelity: every quest checked mechanically (criteria counts, evidence and Result paths
at landing commits); eight checked line by line against their ledgers (`format13`, `x3-1-chunked-registry`,
`phase7c-design`, `phase7c-s1`, `phase7c-http-wave`, `phase7c-task-wave`, `x-pool-step1`, `t11-barriers`).
Live quests read in full: `x-pool`, `wbuf-inline`, `cold-tier-step2`, `manifest-log`, `closing-benchmark`,
`x7-gate`, `barrier-warning-*`; the other 12 by criteria count only.
