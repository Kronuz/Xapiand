# Audit of the xapiand campaign's roadmap (check 0)

Auditor `xapiand-roadmap`, 2026-11-08. Sources: strata `docs/PLAN.md` in all 58 versions
(strata:665677b1 to strata:d93bf7dd), its gate reports and reviews, prospect's gate reports, Xapiand's
`docs/status/road-to-release.md`, `docs/design/xapiand-on-strata.md`, `docs/design/phase7c.md` and
`docs/gates/`, and the migrated campaign on Kronuz/Xapiand `wip/master-migrated-2026-11-08`
(Xapiand:6f7740203) with the drafts in `~/.scratch/migrate/out/xapiand/`. The laptop was not needed;
no etctl session was opened.

## Verdict

The migration recorded the ledger era faithfully but not the roadmap. Of the 18 phases, tracks and
gates in strata's plan, the map shows 4 (Phase 0.5, 7a, 7c, 8); the other 14 (Phases 0, 1, 2, 3, 3R,
4, 5, 6, 7b, Tracks C, P and D, the `document` profile gate, the recovery tooling) have no row, and its
94 history quests sit in 11 milestones named by ledger era ("strata Phase 4", "Phase 7c, landed
steps"), not by the phase they served. Eight completed parts had no history quest: the gates of
Phases 0, 1, 2, 0.5 and 7a, Phase 3's lifecycle rows, Track C's C1 and Phase 0.5's X1; gates 0.5 and
7a were recorded only as validation strings. Nothing is lost: every
phase's evidence is committed in strata, prospect or Xapiand, and the parked branches are on GitHub.
The fixes add 8 history quests and 11 planned or blocked quests, move 73 of the 115 quests, and give
PLAN.md 20 milestone rows in the roadmap's order; rehearsed on clones, `campaign check` is clean and
`campaign map` shows the whole roadmap (see Rehearsal).

## The roadmap, phase by phase

| Roadmap item (strata `docs/PLAN.md`) | State | Evidence | Quests after the fix |
|---|---|---|---|
| Phase 0, baselines and evidence | done 2026-10-11, strata:a2d79a4a; open: `benchkit`, the Xapiand-shaped corpus, the Elasticsearch harness, a query-path profile; hop and light p99 owed on bare metal | prospect `bench/phase0/README.md`; strata `docs/FEASIBILITY.md`; `docs/reviews/phase-1.md` "Phase 0 additions" (recording the RESTORE attribution closes the gate) | `phase0` (new history) |
| Phase 0.5, the engine seam | done 2026-10-27, Xapiand:5b865a059 (run 2 verdict) | `docs/gates/phase0.5.md` | 11 + `x1-analysis`, `gate0-5` (new history) |
| Phase 1, foundation libraries | done 2026-10-11, strata:6a3cddde (gate APPROVED on re-check); open: fst full-vocabulary lookup 2.43x against 2x, the 100,000-pattern automaton test not shown | strata `docs/gates/phase-1.md`, `docs/reviews/phase-1.md` | `phase1` (new history) |
| Phase 2, single-segment engine | done 2026-10-13, strata:6932b812 (APPROVED WITH CONDITIONS); open: allocations per query (C15), termdict p99 on bare metal | prospect `docs/gates/phase-2.md`, `docs/reviews/phase-2-gate.md` | `phase2` (new history) |
| Phase 3, index lifecycle | partly done: lifecycle rows passed 2026-10-14 (strata:2a111373), P1, P6, P12, P15 landed by strata:950d24d (2026-10-17); ratchet rows carried (queries 1.33 to 1.34x, postings 1.014x, terms 1.20x) or owed (indexing T4, write amplification T3, memory, FreeBSD and arm64, parity rows) | strata `docs/gates/phase-3-lifecycle.md`, `docs/reviews/phase-3-lifecycle.md`; prospect `docs/gates/phase-3-ratchets.md` "Gate status (2026-10-16)" | `phase3-lifecycle` (new history), `phase3-ratchets` (planned) |
| Track C, consensus gaps | partly done, parked: C1 gated 2026-10-14 (strata:de99ec7e, APPROVE WITH CONDITIONS); C3 built (consensus afd456d), no gate; C2 and C4 partly built; C5 not started; peer transport revision 2 awaits its re-check; parked by the owner 2026-10-20 (D4), after the release 2026-10-29 (D9) | strata `docs/design/track-c.md`, `docs/gates/track-c-c1.md`, `track-c-c2.md` | `track-c-c1` (new history), `track-c` (blocked, adopts `design/peer-transport`) |
| Track P, performance research | in flight, standing: P1, P6, P12, P15 landed (Phase 3); reclamation in `docs/perf/reclamation.md`; P7 and P21 have entries; 11 items have none | `docs/perf/` of strata and prospect | `research-indexing` (moved), `track-p` (planned) |
| Track D, distribution | parked by the owner 2026-10-20 (D4): D1's design revision 1 in review and S1 to S3 built on `design/simulator` (strata:27cfb3033, 2026-10-17) | strata `docs/status/simulator.md` | `track-d` (blocked, adopts `design/simulator`) |
| Phase 3R, replication groups | future, parked (D4, D9); needs C1 to C4; durability-modes design at revision 10, approved with conditions, on `design/durability-modes` (strata:95357d975) | strata `docs/status/durability-modes.md` | `phase3r` (blocked, adopts the branch) |
| Phase 4, structured data | partly done: format 13 (strata:76fa331), layers, core and repository split, sort keys and collapse (prospect:39aae28), aggregation collectors (prospect:0ac8175), values by slot (prospect:ca67b32), multi-index search (prospect:40fd4bf); not found: `bkd`, geo, delete by query; gate not run | prospect `docs/design/phase-4.md`, `docs/perf/format13-gate-vm.md` | 6 (moved) + `phase4-gate` (planned) |
| The `document` profile gate | in flight: 23 landed quests of strata's core against RocksDB (2026-10-20 to 11-04), `wbuf-inline` and `manifest-log` live; gate not run | strata `docs/perf/core-vs-rocksdb*.md`, `docs/design/format-14-kv.md` ("whose job is to beat RocksDB") | 26 (moved) + `document-gate` (planned) |
| Phase 5, analysis and query language | partly done: queryparser's Xapiand subset on queryparser main (queryparser:56d7e08, 7a's S4), step 3b awaits review; automata on `phase5/automata` (strata:219ded0d7, 2026-10-18, CI red), stalled and on no plan | strata `docs/status/queryparser.md`, `automata.md` | `phase5` (planned) |
| Phase 6, async backends | future: nothing started; 7c's T6 gates here (owner, D15) | strata `docs/PLAN.md` | `phase6` (planned) |
| Phase 7a | done 2026-10-29, Xapiand:0d194335b (gate review REJECT, re-check closes L1, L2, C1 to C4) | `docs/gates/phase7a.md`; strata `docs/reviews/phase-7a-gate.md` | 12 + `fields-and-keys`, `field-table`, `key-field` (moved), `gate7a` (new history) |
| Xapian-only release (owner, 2026-10-28) | partly done: the `xapiand` branch builds (Xapiand:dee2deaad); the final release with backports is owed | the lead's memory, per audit-xapiand | `alpha10-builds`, `xapiand-final-release` (audit-xapiand's) |
| Phase 7b | parked by the owner 2026-10-29 (D9): after the release | strata `docs/PLAN.md` Phase 7 | `phase7b` (blocked) |
| Phase 7c | in flight: 26 steps landed 2026-10-29 to 11-03; X-POOL step 3, the watch-byte read and X7 remain | road-to-release | 29 (three rows merged) |
| Phase 8 | in flight: the license and behavior audits landed 2026-11-03; 13 quests planned or blocked | road-to-release 8a to 8c | 15 (two rows merged) |
| the strata lane (road-to-release) | in flight: cold tier step 1, item 5, step 2 items 1 to 6 and 5b landed; step 2's rest; hybrid parked (owner 2026-10-27, D8) | strata `docs/status/cold-tier*.md` | 12 (`idle-index` and 9 cold-tier quests moved) |
| Recovery tooling | future: after the engine is complete (owner 2026-10-18, D28) | strata `docs/PLAN.md` (named Track H at strata:002737fc) | `recovery-tooling` (blocked) |

Every gate, the Ratchets, the owner's system-review decisions and the names are cross-cutting, not
milestones: they are PLAN.md's Constraints and Goals and Decisions D28 to D32.

## Coverage

| Source item | Where it went | Status |
|---|---|---|
| strata PLAN: Phases 0, 1, 2 | `phase0`, `phase1`, `phase2` | GAP, fixed |
| strata PLAN: Phase 3 (lifecycle rows, P1, P6, P12, P15; ratchet rows) | `phase3-lifecycle`, `phase3-ratchets` | GAP, fixed |
| strata PLAN: Tracks C, P, D; Phase 3R | `track-c-c1`, `track-c`, `track-p`, `track-d`, `phase3r` | GAP, fixed |
| strata PLAN: Phase 4; the document profile gate | the milestones, `phase4-gate`, `document-gate` | GAP, fixed |
| strata PLAN: Phases 5, 6; 7b; the recovery tooling | `phase5`, `phase6`, `phase7b`, `recovery-tooling` | GAP, fixed |
| strata PLAN: Phase 7a, 7c, 8 | existing milestones | OK |
| strata PLAN: Goals, Ratchets, Every gate, owner decisions | PLAN Goals, Constraints, D28 to D32 | GAP (goals missing), fixed |
| xapiand-on-strata path: X1 (analysis swap, Xapiand:fe9307227) | `x1-analysis` | GAP, fixed |
| xapiand-on-strata path: X2 to X4, S1 to S4, SF | existing history quests | OK |
| xapiand-on-strata path: X5 (gate 0.5), S5 (gate 7a) | `gate0-5`, `gate7a`; before, validation text only | GAP, fixed |
| xapiand-on-strata path: P1 to P4 (prospect), L1 (queryparser) | no quest | GAP, for audit-xapiand or audit-libraries; milestone Phase 7a |
| phase7c.md steps: S1, X1 to X6, L5; X7 | existing history; `x7-gate` | OK |
| phase7c.md steps: L1 to L4, L6 (task, threadpool, http and reactor, prospect, storage) | no quest | GAP, for audit-libraries; milestone Phase 7c |
| road-to-release: remaining steps, Perf, 8a to 8c, Before release, the strata lane, carried conditions | existing quests | OK |
| road-to-release: Decisions owed (open questions 1 to 3) | PLAN open questions | stale (2 answered, 3 the lead's), fixed |
| Earlier plans: strata PLAN's first version (strata:665677b1), "Track H" (strata:002737fc), strata `docs/status/README.md` queues (2026-10-17, 10-25) | every phase now a row; queue items cited as prior evidence in `track-c`, `track-p`, `phase3-ratchets` | OK after the fix |
| Earlier plans in Xapiand's history | none exists before road-to-release (Xapiand:4b37d3a67) | OK |
| Parked branches `design/simulator`, `design/peer-transport`, `design/durability-modes` | adopted by `track-d`, `track-c`, `phase3r` | GAP, fixed |
| Parked branches `phase5/automata`, `phase3/bisection-pool`, `design/query-gap` | named in `phase5` (open question 11), `track-p`, `phase3-ratchets` | GAP, the lead adopts or drops |

## Gaps, by severity

### Lost work or evidence

None lost. Two risks:

1. Six parked strata branches carry unmerged work and belonged to no quest: `design/simulator` (21
   commits: the simulator's engine, network and checker), `design/durability-modes` (83: the model,
   schedules and results), `design/peer-transport` (18), `phase5/automata` (14: the automata code),
   `phase3/bisection-pool` (4) and `design/query-gap` (17). A cleanup of unadopted branches would drop
   them. Fix: `map-xapiand.sh` adopts the first three as `quest/xapiand/{track-d,phase3r,track-c}`; the
   other three are named as prior work in `phase5`, `track-p` and `phase3-ratchets` for the lead.
2. Phase 0's oracle harness `strata-oracle` (GPL, kept outside the repository by design) is cited by
   the Phase 1 re-check ("strata-oracle/vectors") and serves Phase 2's parity and Phase 5's stemmer
   gate; `gh repo view Kronuz/strata-oracle` finds no repository. Where it lives on the laptop is
   check 3's (audit-xapiand).

### Missing history

3. The map has no row for 14 of the 18 roadmap items (table above). Fix: PLAN.md's Milestones table,
   20 rows in the plan's order (the 18, the Xapian-only release after 7a, the strata lane after Phase 8).
4. Completed work with no history quest: Phases 0, 1 and 2 (strata:a2d79a4a, 6a3cddde, 6932b812),
   Phase 3's lifecycle rows with P1, P6, P12 and P15 (strata:950d24d), Track C's C1 (strata:de99ec7e),
   Phase 0.5's X1 (Xapiand:0cf555fac), and gates 0.5 (Xapiand:5b865a059) and 7a (Xapiand:0d194335b).
   Fix: 8 `hist` lines; criteria are each plan section's items and gate rows, evidence the committed
   gate reports and reviews; 11 criteria stay open where the record does not show them (phase0 A3 to
   A6, phase1 A8, A11, A12, phase2 A16, phase3-lifecycle A10, track-c-c1 A9, gate7a A8).
5. Future and parked phases had no quest: 11 prepared, criteria from the plan only; 5 blocked with the
   owner's reason (`track-c`, `track-d`, `phase3r` by D4 and D9, `phase7b` by D9, `recovery-tooling` by
   D28), 6 planned, ordered by `needs` and `after` where the plan orders them (`phase3r` needs
   `track-c`; `phase7b` needs `release`, `phase3r`, `track-d`; `phase4-gate` after `phase3-ratchets`;
   `document-gate` after `phase4-gate` and `manifest-log`). Gate 7a's "Not run here" items (Linux,
   FreeBSD and arm64 sanitizer runs, parity rows, the scale statement, the fuzz regression corpus) were
   carried to "the 7b gate" in prose only; `phase7b` A8 now holds them.

### Fidelity errors

6. 73 of 115 quests sit in milestones named by ledger era. Examples: `fields-and-keys`, `field-table`
   and `key-field` served gate 7a (strata `docs/design/fields-and-keys.md`: "SF strata adapter on fields
   -> S5 gate 7a") but sit in "strata fields and keys"; `format-14-kv` and `f14-step1` state their goal
   as the document profile gate yet sit in "strata Phase 4". Fix: `moves-xapiand.sh`, rules below.
7. Phase 8's validation is cut at the `|` inside its regex, because PLAN.md's table splits cells on
   `|`: the map shows "`! git grep -n -i -E 'sortable_serialise". Fix: three `-e` patterns.
8. Non-goals listed 7b and the parked distribution work, which the owner parked (D4) and sequenced
   after the release (D9), never dropped; the Goals omitted strata's five engine goals. Fixed.
9. Open questions: 2 answered (D33) and 8 answered (D30), 3 is the lead's call, 4 lacks the owner's
   non-AWS plan (audit-xapiand's findings, applied); missing: the bare-metal and arm64 hosts (9),
   whether the Phase 3 to 6 and document gates precede the release (10), where automata lives (11).
10. Decisions the roadmap rests on were missing: D28 (recovery tooling after the engine), D29 (carried
    rows), D30 (formats open until the release), D31 (build ahead of the gates), D32 (engine priority);
    D33 and D34 come from audit-xapiand.

### Cosmetic

11. The brief's list of phases omits Track P (in the plan since strata:c8966ef0, 2026-10-12) and the
    recovery tooling (since 2026-10-18); both are rows.
12. 7b keeps the plan's position between 7a and 7c although it runs after Phase 8 (its blocked reason
    says so); "the strata lane" keeps its lower-case name so its live quests need no move.

## Judgment calls for the lead

- Moves: a quest goes where its own design or ledger names its gate; otherwise strata core work after
  the repository split (2026-10-20), measured against RocksDB, goes to the document profile gate (the
  lead's priority 3, 2026-10-17; "snapshot publication" is that gate's own prerequisite, which
  `shared-reader-*` and `write-buffer-reads` built); `phase4/` format and layer work to Phase 4;
  `research-indexing` to Track P (the 4.5x indexing target); `idle-index` to the strata lane (the cold
  tier's measured basis); `manifest-log` and its steps to the document gate (its D9: standalone
  durability, rocksdb-survey K2); `wbuf-inline` with the write buffer; `alpha10-builds` stays in
  "Xapian-only release".
- Phase 3 is two quests: the lifecycle rows and landed P items as history (recorded at strata:950d24d,
  the last of them; the lifecycle approval is strata:2a111373), the ratchet rows as planned.
- Phase 2 and the Phase 3 ratchets moved to prospect with the search code; the history lands on
  strata's commit, the planned gate quests are prospect's.
- Gates whose rows passed with the architect's approval but missed a plan target stay open on that
  criterion (Phase 1's fst lookup at 2.43x): never weaken a criterion.

## Rehearsal

On clones of Xapiand `wip/master-migrated-2026-11-08`, strata `wip/main-migrated-2026-11-08` and
prospect, http, completion and sortable `main`, with the installed `campaign` and an isolated job
store (`CAMPAIGN_JOBS`): `map-xapiand.sh`, `history-xapiand.sh` (one commit per
step, 42 commits with the map script) and `moves-xapiand.sh` with `ONE_COMMIT=1`, then all three again.

- `campaign check`: ok. The second run changes nothing (HEAD unchanged; 14, 8 and 115 "already" lines).
- `campaign map` before: 94 of 115 quests landed, 516 of 716 criteria met, 14 milestones in ledger-era
  order, Phase 8's validation cut. After: 102 of 134 landed, 593 of 910 met, the 20 milestones in the
  roadmap's order (Phase 0, 1, 2 and 7a done; Phase 3 1 of 2; Track C 1 of 2; Track P 1 of 2; Track D
  and Phase 3R blocked; Phase 4 6 of 7; the document gate 23 of 27; Phases 5 and 6 planned; 7b blocked;
  7c 25 of 28; Phase 8 2 of 15; the strata lane 10 of 12; the recovery tooling blocked). Every quest
  appears once, none outside a milestone, and each new history quest's open criteria equal its
  journal's "Left open" line. "Xapian-only release" reads done only because audit-xapiand's
  `xapiand-final-release` was not in the rehearsal.
- Maps kept beside this file: `rehearsal-map-before.md`, `rehearsal-map.md`.
