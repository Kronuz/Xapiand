# phase3-ratchets: Phase 3's ratchet rows

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status phase3-ratchets`.

## Outcome

Phase 3's gate closes: its ratchet rows on query speed, indexing speed, memory and disk are measured
on the gate VM at a main commit and pass, or stay carried by the owner with their measured ratio, gap
and the work that closes them; `verify` and `parity` pass on multi-segment indexes; coverage and the
test-parity inventory's Phase 2 and Phase 3 rows are met; and the rows run on FreeBSD and arm64
before Phase 3R. The lifecycle rows passed already (`phase3-lifecycle`). Phase 4's gate re-measures
these rows and does not pass while a carried row misses its original target (strata `docs/PLAN.md`,
"Carried rows"). Repository: Kronuz/prospect, which holds the search engine, the harness and the
report since the repository split.

## Acceptance criteria

From strata `docs/PLAN.md` "Phase 3" (gate) and "Ratchets" (the Phase 3 rows), with the state of each
row in prospect `docs/gates/phase-3-ratchets.md` ("Gate status", 2026-10-16). Timed rows: one `exclusive` job
per row on a main commit, by D59.

- A1 Queries after merging and before merging (baselines unmerged at equal memory): every class at most 1.0x the fastest of Lucene, Lucene with reordering and Tantivy; carried at 1.33 to 1.34x global after merging (owner, 2026-10-15); format 13's run put strata at 54 wins and 17 losses against Lucene `-bp` (prospect `docs/status/f13-gate.md`): gate VM jobs
- A2 Single-thread indexing per index, keyed, at equal memory (T4), at least 1.0x the faster of Lucene and Tantivy, with the 8-thread row; owed since 2026-10-16 (calibrated at 5M): gate VM jobs
- A3 Write amplification against Lucene's tiered policy at equal segment budgets, append-only and replace-heavy (T3); the many-index indexing must-win rows, one thread per index: gate VM jobs
- A4 Memory: indexing within the node budget plus 10% and at most 1.0x; serving at most 1.0x (1.05x Tantivy on 2026-10-16); allocations per query do not grow with the number of matches: gate VM jobs
- A5 Disk after a full merge: the whole index at most 1.0x (0.999x keyed `-bp`, passed) and every section at most 1.0x its counterpart (postings 1.014x and terms 1.20x carried to format 13; format 13 left postings 9.96 MB over keyed `-bp`'s `.doc`, its D15): gate VM jobs
- A6 `verify` and `parity` on multi-segment indexes before and after merging, `parity` with deletes included: jobs
- A7 [artifact] Coverage targets and the test-parity inventory's Phase 2 and Phase 3 rows, owed rows named: `docs/testing/parity.md`
- A8 The rows on FreeBSD and on arm64, required before Phase 3R (the M1 is retired; an arm64 host is owed): jobs on the FreeBSD VM and an arm64 host
- A9 [artifact] Tools at Phase 3's start: Lucene with reordering in the harness, an allocation-counting build, the cgroup memory tool, and a runner that scores all four axes (strata `docs/BENCHMARKS.md`): `docs/gates/phase-3-ratchets.md`
- A10 [artifact] The Phase 2 gate's conditions C1 to C3, C5, C7, C9 and C13 to C16 met by their stated points (prospect `docs/reviews/phase-2-gate.md`, "Conditions"), each with where it closed: `docs/gates/phase-3-ratchets.md`
- A11 [artifact] The gate report: every row with its ratio, interval, commit and platform, and each carried row with its gap and the work named to close it: `docs/gates/phase-3-ratchets.md`

## Contracts

None: measurement; a row that fails is fixed by its own quest (Track P, `phase4-gate`) and measured again.

## Decisions

- Carried rows (strata `docs/PLAN.md`, "Ratchets"): the query rows carried at Phase 3 (owner, 2026-10-15); the term dictionary and the postings section carried to format 13 (2026-10-16, the owner's call delegated: disk rows may carry). D29.
- Targeted runs (owner, 2026-10-17): iterate on about 100,000 documents, calibrate once, then time strata against the fastest engine only; one scale-up confirmation is the gate row.
- PISA is reported beside every Phase 4 row and binds from Phase 6 (QG-R28, F13-R45).
- The arm64 host for A8: none beyond GitHub's CI (D36), so its arm64 rows wait for one.
- Follows the release (D39), except A6, which `release-rows` proves before it; this gate re-runs A6
  at its own commit.
- Prior evidence, not to redo: prospect `docs/gates/phase-3-ratchets.md` (rows of 2026-10-14 to 17), `docs/status/f13-gate.md` and `docs/perf/format13-gate-vm.md` (format 13 at 5M on the gate VM), strata's query-gap design on `design/query-gap` (attribution predates the prospect query wins; take targets from a fresh measurement).

## May land early

The harness and report changes.
