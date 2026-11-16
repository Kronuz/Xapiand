# Libraries audit: the Kronuz library repositories after the 2026-11-08 migration

Auditor: `libraries`; etctl session `remote-c5c46609-libraries-90ab36` (left open). Scope: the 94
repositories under `~/code/Kronuz` on the laptop other than Detent, Xapiand and strata (`btree` is a
symlink to `cpp-btree`; the `-wt` directories are worktree containers), checks 1 to 4, plus the loose
files in `~/code/Kronuz`, which is not a repository (no `.git` there or above it).

## Verdict

No branch was lost, but the library half of both campaigns was not migrated. All 260 local branches
are on GitHub at the same sha (103) or as an ancestor of a GitHub branch (152); the only exceptions are
the four branches of `strata-oracle`, a repository with no remote at all, and `queryparser-review`'s
`main`, whose commit is on Kronuz/queryparser. Of the 52 library ledgers, none had a history quest
(two were represented otherwise: prospect `term-lists` by decision D7, completion `sync-files` by Detent
`namespace-s1-s2` A1-A2); the Xapiand report names 11 as unmapped (question 26's five 7c ledgers, five
moved to prospect, `library-issues`) and the other 41 appear nowhere. This audit prepares 50 history quests (41
Xapiand, 9 Detent; 319 criteria, 35 left open exactly as the ledgers left them), rehearsed end to end
with `campaign check` clean. The lost-evidence risks are small in bytes and large in consequence: a
code review two landed ledgers cite exists only uncommitted on the laptop; the Phase 0 Xapian oracle
(`strata-oracle`, 17 commits, the vectors prospect's F8 parity used) exists only on the laptop; two
documents committed docs cite (`KRONUZ_REFACTORING_DIRECTIVES.md`, the extraction backlog) are
unversioned files; W2a's design revision 2 and its review are only on an unmerged http branch; and 13
Codex-era worktrees under `Detent/.scratch` hold 63 uncommitted library files not on GitHub (mostly
rejected btree trials and drafts later superseded).

## Counts

| Item | Count | Result |
|---|---|---|
| Repositories in scope | 94 (92 with a GitHub remote) | `ls-remote` from this Mac for all 92 |
| Local branches | 260 | 103 same sha, 152 ancestors, 5 no remote (strata-oracle 4, queryparser-review 1: on GitHub queryparser) |
| Detached worktree heads and stashes | 108 | 105 on GitHub; 3 local-only (below) |
| Worktrees | 319 | 22 with uncommitted changes outside build output: 7 fully on GitHub; 15 not (13 Codex-era under `Detent/.scratch`, the stash review, fusefilter's untracked `readme_snippet.cc` and build files) |
| Branches not merged into GitHub main | 29 in 17 repos | "about 40" counts laptop mains, 81 of which are behind GitHub |
| Library ledgers (`docs/status/*.md`) | 52 in 16 repos | 0 history quests before; 49 history quests prepared for them (+1 for unledgered sha256:07cdc83); 1 planned quest; 1 decision (D7) |
| Library reviews | 40 committed outside cpp-libraries (+1 uncommitted), 79 in cpp-libraries | cited by the prepared journals or listed below |

## Coverage: ledgers

Owner, by the ledger's text, branch and the leads' plans. "H" is a prepared history quest
(`fixes/<Home>/.campaign/<c>/quests/<id>.md`, journal, `hist` line); every one was GAP before.

| Ledger | Owner, why | Became | Status |
|---|---|---|---|
| task `phase7c-l1` | Xapiand: 7c step L1 | H `task-phase7c-l1` (Phase 7c), task:9a85e89 | GAP, fixed |
| threadpool `phase7c-l2` (+ `scheduler.md`, superseded) | Xapiand: L2 | H `threadpool-phase7c-l2`, threadpool:e961e44; A10 (CI), A15 (K6) open | GAP, fixed |
| threadpool `research-scheduler` | Xapiand: L2's research | H `threadpool-research-scheduler`, threadpool:394aaea | GAP, fixed |
| http `phase7c-l3` (reactor, http-log, prism) | Xapiand: L3 | H `http-phase7c-l3`, http:4d56ab1 | GAP, fixed |
| prospect `phase7c-l4` | Xapiand: L4 | H `prospect-phase7c-l4`, prospect:c77d6eb | GAP, fixed |
| storage `phase7c-l6` | Xapiand: L6 | H `storage-phase7c-l6`, storage:c3d3efb (follow-up storage:e4c5746) | GAP, fixed |
| reactor and http S1 (no library ledger) | Xapiand | history `phase7c-s1` A1 (reactor LagProbe) | OK |
| threadpool, scheduler `unify-pool`; logger, prospect `drop-threadpool` | Xapiand: one pool, 7c | H `threadpool-unify-pool`, `scheduler-unify-pool`, `logger-drop-threadpool`, `prospect-drop-threadpool` | GAP, fixed |
| prospect `pins-2026-11` | Xapiand: pin wave | H `prospect-pins-2026-11` | GAP, fixed |
| msgpack `unpacker-limits` | Xapiand: RESTORE hardening (phase7c-linux-gate-fixes item 3) | H `msgpack-unpacker-limits`; A7 (corpus measurement) open | GAP, fixed |
| prospect `inverter`, `inverter-followups` | Xapiand: Track P, strata before the split | H `prospect-inverter`, `prospect-inverter-followups` | GAP, fixed |
| prospect `f13-query`, `f13-gate`, `f13-preflight` | Xapiand: format 13 | H `prospect-f13-query`, `-f13-gate`, `-f13-preflight` | GAP, fixed |
| prospect `query-wins`, `pruning-floors`, `perf-phrases`, `word-sum-memory` | Xapiand: Track P | H, one each | GAP, fixed |
| prospect `values-bytes`, `xapian-ops`, `host-collectors` | Xapiand: xapiand-on-strata P1, P3, P4 | H, one each (Phase 7a) | GAP, fixed |
| prospect `term-lists` | Xapiand: P2, REJECTED | decision D7 (R-P2) in Xapiand PLAN; branch `phase4/term-lists` kept | OK |
| prospect `fields`, `key-field`, `phrase-alternatives` | Xapiand: F2, F3, gate 7a L1 | H, one each | GAP, fixed |
| termdict `forward-seek`, `validate-at-open` | Xapiand: engine library | H `termdict-forward-seek`, `termdict-validate-at-open` | GAP, fixed |
| hashes `cl-1`, completion `cl-2`, sha256 `cl-10`, hashes `fold32` | Xapiand: the 2026-10-19 consolidation audit (strata lead), compact-slots | H, one each (Library family) | GAP, fixed |
| stash `perf-window-epoch`, scheduler `perf-lazy-rearm`, logger `fix-refused-add` | Xapiand: the debounce-refusal work with Xapiand:903ad2350 | H `stash-window-epoch`, `scheduler-lazy-rearm`, `logger-refused-add` (Phase 0.5) | GAP, fixed |
| logger `producer-path`, `producer-rings`; stash `claim-hint-d64`, `claim-hint-publish` | Xapiand by inheritance: run in the October library work outside both leads; Xapiand's lead owns its issue queue | H, one each (Library family); open: criterion 1 of each logger ledger, d64's bar | GAP, fixed |
| cpp-libraries `library-issues-2026-11` | Xapiand: its "library queue" (told to Detent's lead, 2026-11-02) | planned quest `library-issues`, blocked until after the release | GAP, fixed |
| sortable `sortable` | Xapiand: sortable-codec step 1 | H `sortable-step1` (Phase 8); its Next is live `sortable-proof` | GAP, fixed |
| task `cancellation` | Detent: owns task's change (decision, November 2) | H `task-cancellation` (M1); K3, K5, K6, K7 open | GAP, fixed |
| reactor `task-sessions` | Detent: owns reactor's change | H `reactor-task-sessions` (M1) | GAP, fixed |
| http `task-handlers`, `task-h1`, `task-h2`, `affine-thread` | Detent: W2a | H, one each (M1); umbrella K8, K9 open, K1-K5 cite the step ledgers | GAP, fixed |
| completion `task-driver` | Detent: W2 Q6 | H `completion-task-driver` (M1) | GAP, fixed |
| completion `sync-files` | Detent: namespace S1 | H `completion-sync-files` (N1); `namespace-s1-s2` A1-A2 cite it | GAP, fixed |
| (no ledger) sha256:07cdc83, AVX2 kernel, 2026-11-07 | Detent: the owner asked the Detent overseer for first-read item 5 | H `sha256-avx2` (M3) | GAP, fixed |

## Coverage: reviews without a ledger, and branches

| Item | Where it belongs | Status |
|---|---|---|
| prospect `docs/reviews/phase-2-gate.md`, `phase-4-design.md`, `p6-multisegment.md` | strata's Phase 2 gate, Phase 4 design, Track P P6 (reviews moved with the split) | GAP for the Xapiand auditor's phase history quests: cite them |
| prospect `count-union-bits.md` | Track P counted unions (prospect:54e3cc0) | cited by `prospect-word-sum-memory`; the work itself has no quest (minor) |
| journal `framing-security.md` | strata `manifest-log-steps12` | that journal's Result does not name it (minor, Xapiand auditor) |
| task `sync-wait.md`, termdict `seek-decode.md`, stash `descent-window-code.md`, reactor `udp-receive-loop-code.md` | Phase 1 / Track P library work of Oct 17-21 | no quest; the UDP item is a Detent SHARED_LIBRARY_DECISIONS row (cosmetic) |
| cpp-libraries 79 reviews (surveys, audits, system review 2026-10-30/31, gate 2026-11-01, uinteger_t/base-x) | the October library work, run outside both leads (the Xapiand lead calls them "the cpp-libraries gate's numbers, not ours") | its open issues: `library-issues`; consolidation-2026-10-19 is cited by the CL quests |
| http `design/task-handlers` (http:694fb47) | W2a design revision 2 and its review, cited by 4 Detent quests | GAP: docs only, prepared for main |
| http `track/http-integration`, http-parser, compressors, allocators same name; msgpack `track/bounded-integration` | Detent EX-2, EX-3 sources | msgpack named (msgpack-adoption A2), http named (A1); the other three GAP, prepared |
| prospect `explore/phrase-speed` (Nov 4) | Track P loser record | GAP: `docs/perf/phrase-speed.md` prepared for main |
| completion `cl-2-detent-replay`, cpp-libraries `ccache`, cluster `document-is-alive-contract`, queryparser `wip/laptop/*` | patch-equivalent to main | OK |
| completion `phase/sync-files`, storage `phase7c/l6-streamed-write`, journal `cl-1-detent-replay`, hashes `cl-1-crc32c-leaf` | superseded: relanded, cherry-picked, or the CL-1 ledger's rejected design | OK (named in the quests) |
| prospect `explore/req-opt`, `explore/topk-count`; termdict `explore/forward-seek-a/-b`; docset `explore/bitmap-and`; termdict `explore/point-lookup` | explorers' records | OK via pruning-floors, forward-seek, `library-issues` |
| prospect `phase4/term-lists`; http `trace/hop` | D7; `x7-gate` and branch-triage | OK |
| prospect `quest/xapiand/wbuf-inline-prospect`, `test/wbuf-inline-flushes` | live `wbuf-inline-prospect` | OK |
| stash `wip/certain-reclamation`, `wip/sentinel-frontier`; cluster `fix/raft-delegate-prefers-docs`; fantasyname `gh-pages` | before both campaigns (June-September) | not owned; on GitHub |

## Gaps, ranked

### 1. Lost work or lost evidence

1. **`strata-oracle` has no remote.** `~/code/Kronuz/strata-oracle`, branches `main` (699ee7d),
   `ops-vectors` (6750aef), `shard-vectors` (25be4a3), `phase8/rank-terms` (8e6611a); 17 commits since
   2026-10-11; 27 tracked files; loose objects only. It is strata PLAN Phase 0's "GPL-side oracle harness,
   outside the strata repository". prospect commits the ops and shards vectors as fixtures, but not the
   `rank-terms` vectors that `prospect-fields` F8 (T3b parity, worst error 4.1e-16) ran against
   (`tools/parity_terms.cc` takes them as an argument), nor the generators. Fix: the owner creates a
   repository for it (GPL; a new repository is his call) and the laptop pushes all four branches.
2. **An uncommitted review that two landed ledgers cite.** `~/code/Kronuz/review-wt/stash/docs/reviews/window-refusals-code.md`
   (2026-10-20, 15,453 bytes, blob eb50589, APPROVE WITH CONDITIONS over stash, scheduler, logger and
   Xapiand) is untracked in a detached review worktree; stash `docs/status/perf-window-epoch.md:38` and
   scheduler `docs/status/perf-lazy-rearm.md:63` cite it. Fix: `fixes/stash/docs/reviews/window-refusals-code.md`
   (byte-exact).
3. **Cited documents outside every repository.** strata `docs/PLAN.md:661` and Xapiand
   `docs/dev/extraction-ledger.md:5-6` cite `~/code/Kronuz/KRONUZ_REFACTORING_DIRECTIVES.md` and
   `~/code/Kronuz/memory/kronuz-extraction-backlog.md` by laptop path. Fix: prepared as cpp-libraries
   `docs/REFACTORING-DIRECTIVES.md` and Xapiand `docs/dev/extraction-backlog.md` (two work-account
   mentions removed), with both citing docs corrected.
4. **Design of record off main.** http main has only revision 1 of `docs/design/task-handlers.md`, which
   the architect rejected; revision 2 (what H1, H2, R and C1-C3 built) and `docs/reviews/task-handlers-rev2-design.md`
   are only on `design/task-handlers` (http:694fb47, docs only). Fix: both prepared for http main.
5. **Uncommitted library work in Codex-era worktrees** under `~/code/Detent/.scratch/` (Detent auditor's
   directory, these repositories): 16 dirty worktrees, 13 of them holding 63 files whose content is on no
   GitHub branch (blob ids compared), Oct 17-29. cpp-btree 45
   files in 5 worktrees (`cpp-btree-compact-copy`, `-erase-transaction`, `-exception-safety`,
   `-insertion-transaction`, `-moves`: Oct 20-21 trials of the representation the architect rejected;
   the Oct 22 trials' files are on GitHub); scheduler 9 (`scheduler-wakeup-20261029`, superseded by
   scheduler:36afada and 34631ec on main, except the probe `test/wakeup.cc`); compressors 3, http 3,
   http-parser 1, msgpack 1, consensus 1 (README, docs and one test of drafts whose code files are on
   GitHub). Local-only commits: cpp-btree 7d3f35d (with c54ff57, 06f1b6f) in `cpp-btree-isolated`;
   consensus `stash@{0}` b40bdb5 (2026-10-15, removes `turn_journal.h`); allocators ebe16c6 has a twin,
   bd8ca74, on GitHub. Fix (the laptop's owner): commit each worktree's state to a `wip/laptop/<name>`
   branch and push, or drop them on record; Detent's container-selection history also cites btree
   44cb140, which is neither on GitHub nor in the laptop's cpp-btree.

### 2. Missing history

6. **49 completed library ledgers had no history quest** (above; the 50th quest is gap 9). Fix: `fixes/history-xapiand.sh` (41) and
   `fixes/history-detent.sh` (9), run after map.sh and history.sh, `ONE_COMMIT=1` for one commit.
   Rehearsed on scratch clones with an isolated `CAMPAIGN_JOBS`: every quest recorded, rerun idempotent,
   `campaign check` ok; Xapiand's map goes from 94 to 135 landed. Before running, fast-forward the
   laptop's `../http` (needs http:fd29d35) and `../completion` (completion:ab0f551); the script stops
   with that message otherwise. Each quest's repository is `../<lib>`; each journal cites the library's
   own ledger, which exists at its landing commit.
7. **Milestones the roadmap does not yet have.** The quests use the roadmap's names: Phase 7c (12),
   Phase 7a (7), Track P (7), Phase 4 (3), Phase 0.5 (3), Phase 8 (1), and "Library family" (8) for the
   consolidation audit and library upkeep, which no phase lists; Detent's use M1 (7), N1 (1), M3 (1).
   The Xapiand auditor's corrected PLAN needs rows "Track P" (strata PLAN has it; the brief's list
   omits it), "Phase 4" and "Library family", or `campaign set ID milestone=...` to its names.
8. **`library-issues-2026-11` had no quest.** Five open rows (termdict#1, logger#1, journal#1,
   allocators#1, waves 1-3) were carried nowhere. Fix: planned quest `library-issues`
   (`campaign new library-issues --title "The library performance issues the cpp-libraries gate filed"
   --risk normal --milestone "Library family" --repo ../cpp-libraries`, then `campaign set
   library-issues status=blocked reason="owner 2026-11-02: after the release"`).
9. **Unrecorded landed work:** sha256:07cdc83 (2026-11-07, AVX2 kernel, 1.6x the portable kernel at
   64 MiB on the i9, CI 42376213584 green) on the owner's word to the Detent overseer; no ledger.
   Fix: history quest `sha256-avx2` (Detent M3). The first-read note it answers is Detent:fcb5c4b on a
   laptop-local branch (Detent auditor).

### 3. Fidelity errors

10. **`perf-watch-peek` does not name its prior evidence.** The "keep-alive watch byte's extra read"
    was changed twice after L3's note: http:9f4c93f (read-ahead watch, 10-31) and http:c37474a (lazy
    watch, 11-01), measured in http `CHANGELOG.md`: 87,316 to 93,657 req/s at 8 connections, A/A 0.95.
    Fix: Decisions amended (criteria unchanged); the lead decides whether A1 and A4 are already met.
11. **`sortable-proof` does not cite sortable's ledger**, whose results it continues (50.9M property
    values, 83.9M exhaustive short inputs, 279M fuzz runs in 40 CPU-minutes). Fix: Decisions amended.
12. **Detent `http-client-adoption`** names http:bd3e8e1 but not the http-parser, compressors and
    allocators `track/http-integration` branches its EX-2 reconciliation consists of; **`pin-wave`** does
    not carry threadpool's "Note for the Detent lead" (the `WorkPool` equivalents the wave must apply).
    Fix: Decisions amended in both.
13. **Ledgers left stale at landing**, recorded as such in the journals: task `phase7c-l1` and prospect
    `phase7c-l4`, `host-collectors`, `key-field`, `query-wins` still read "in review"; CI rows of
    L2 (L2-10), termdict K5, forward-seek and fold32 were never recorded. No criterion was ticked by
    this audit; each stays open where the ledger left it unticked.

### 4. Cosmetic

14. `phase7c-s1`'s journal does not name the reactor and http commits of S1 (reactor:ef26c30,
    http:c23bb99); Xapiand's report question 26 lists "reactor and http S1" as untreated, but `phase7c-s1`
    A1 covers it.
15. Laptop mains are behind GitHub in 81 of 91 repositories (ancestors, never ahead): "about 40
    repositories with unmerged branches" matches the 35 repositories with branches not merged into
    those stale mains; against GitHub main, 29 branches in 17 repositories are unmerged.
16. `prospect` `explore/phrase-speed` keeps its losers' record off main. Fix: `docs/perf/phrase-speed.md`
    prepared.

## Results outside git (check 3)

- 379 `.scratch/` citations in library ledgers, reviews and perf docs (prospect 140, task 75, threadpool
  46, stash 38, http 26, termdict 19, reactor 14, others 21): about 70% are mutant scripts, probes and
  build directories; the rest are raw logs and tables whose numbers the citing committed doc carries.
  Raw data that matters is already committed: threadpool `docs/status/phase7c-l2/` (38 files) and
  `docs/research/scheduler/` (33), scheduler `docs/status/unify-pool/`, http `docs/perf/t6-t7-2026-11-03/`,
  prospect `docs/perf/format13-gate-vm.sha256` (hashes of the 32 MB on the gate VM).
- Not durable and depended on: the stash review (gap 2) and the oracle vectors (gap 1). prospect
  `docs/status/fields.md` F2 cites `.scratch/t1/t1.txt` and `ident.txt`; the ratios and the identity
  result are in the ledger itself, so nothing further is prepared.
- Library `.scratch` directories total several GB (prospect worktrees 0.1-1.1 GB each, consensus 0.8
  GB), and `~/code/Kronuz/.scratch-archive/` (2026-10-28) keeps the `.scratch` of cleaned worktrees:
  prospect 1.7 GB (host-collectors, perf-phrases, explore-phrases, xapian-ops, values-bytes, word-sum and
  two more), strata 2.3 GB, Xapiand 1.8 GB. These are the raw files the prospect ledgers' `.scratch/`
  citations name; their numbers are in the committed ledgers and perf docs. Build trees and corpora, not
  summaries: nothing else worth committing was found.

## Loose files in `~/code/Kronuz` (not a repository)

| File | What it holds | In a repository? | Where |
|---|---|---|---|
| `KRONUZ_REFACTORING_DIRECTIVES.md` (2026-07-02) | the project-agnostic extraction doctrine | no; cited by strata PLAN and Xapiand's extraction ledger | cpp-libraries `docs/REFACTORING-DIRECTIVES.md` (prepared) |
| `HANDOFF-http-server-lib.md` (2026-07-01) | handoff of http becoming a server library and Xapiand's search as one handler (Leg 2) | no; the work has since landed (L3, X3, W2a) | http `docs/history/2026-07-server-library-handoff.md` (prepared) |
| `AGENTS.md` (2026-07-12) | workspace entry point for agents: memory index, identity and push rules (`~/Development/Kronuz` paths) | no | personal: it names the owner's work GitHub account; not for a public Kronuz repository; the owner picks a private home or leaves it |
| `AGENT_CONTEXT.md` (2026-06-28) | a context export: stash/scheduler concurrency arc, Xapiand branches, extraction backlog | no; its library facts are in stash, scheduler and the backlog | same as AGENTS.md (work account named) |
| `session-recap.md` (2026-07-12) | narrative of the June-July sessions | no | personal (a work-PTO note and internal-blog data); not for a public repository |
| `memory/` (7 notes), `session-archive/` (76 MB events, 30 checkpoints, a SQLite DB) | agent memory and a preserved Copilot CLI session (June-July) | no | `memory/kronuz-extraction-backlog.md` prepared (cited); the rest is pre-campaign personal state, no campaign evidence depends on it |
| `.scratch-archive/` (2026-10-28, 5.8 GB) | the `.scratch` of cleaned prospect, strata and Xapiand worktrees | no | raw results; see check 3, nothing to commit |

## For the other auditors

- Xapiand: add milestone rows (gap 7); phase history quests should cite prospect's
  `phase-2-gate.md` and `phase-4-design.md`; `manifest-log-steps12` can name journal
  `docs/reviews/framing-security.md`.
- Detent: the 16 dirty library worktrees and btree 44cb140 (gap 5) sit in `~/code/Detent/.scratch`;
  Detent:fcb5c4b (the first-read note) is laptop-local.
