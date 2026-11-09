# closing-benchmark: Phase 8b's closing benchmark, run once and published

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status closing-benchmark`.

## Outcome

One benchmark, once, before the release: Xapiand on strata (S) against Xapiand on Xapian (X, the
final Xapian-only release), Elasticsearch (E) and Quickwit (Q) through each system's HTTP API on
every axis the release claims, plus strata against Xapian 2.1.0 alone, with equal cores, memory and
disk, result caches off, versions pinned, and losses published as losses
(`docs/design/closing-benchmark.md` revision 1; D1, D23).

## Acceptance criteria

The ledgers' done criteria were never set ("K1 (to be set by the lead)" in both
`closing-benchmark.md` and `closing-bench-harness.md`); these carry the design's gate rows and the
harness ledger's Next list. The lead confirms or amends them before the quest starts.

- A1 The harness's offline tests pass (`test_closing_harness.py`), `ruff` and `pyrefly` clean (harness ledger, Proof): the `harness` check
- A2 T8 to T10 units (`closing_tenants.py` end to end per shape, B4/B5 tenant sets built once and restarted on), E1 to E4 report units, `--until-minutes` chunking of B1/B2, the tune step writing `settings.json`, the X binary build of the `xapiand` branch in `setup.sh` (harness ledger, Next): the `harness` check
- A3 C-wiki's sha256 and 5,032,104-line count recorded in `corpora.pins` (harness ledger, Next): the `harness` check
- A4 The dry run (D9, D14's eleven units) and its ledger section, on the host the owner allows (design step 6; PLAN open question 4): jobs
- A5 T1, T2 indexing (documents per second to searchable, CPU-seconds per 1,000 documents), five rounds: jobs
- A6 T3 index size (natural bytes the gate figure; WAL, translog and metastore as their own column): jobs
- A7 T4a, T4b query latency by class and mode, CPU-seconds per query, client floor beside: jobs
- A8 T5a, T5b query throughput (p99 at or under the class bound, achieved within 1% of offered): jobs
- A9 T6 mixed read and write, visibility lag at 10 ms resolution: jobs
- A10 T7 restart and recovery, clean and crash, with `engine_crash_check.sh`'s audit and D5's windows; Quickwit's post-crash publication wait stated (harness ledger: the lead decides whether it is a result): jobs
- A11 T8 to T10 memory per open index, cold open, wide search, at N_min and each shape's full count: jobs
- A12 T11 serving memory; E's smallest heap reported, not ranked: jobs
- A13 T12 engines alone: prospect's ratchet rows, strata against Xapian 2.1.0 (and Lucene, Tantivy) (D12 of the design): jobs in prospect
- A14 T13 validity on every run (generator headroom, A/A deviation, cache counters, count modes, effective settings, host identity, `fio`); a failed assertion voids the unit: every run's output
- A15 [artifact] The published results, every row with its runs, losses as losses (D23): `docs/perf/closing-benchmark.md`

## Contracts

Measures the release candidate: S at the commits D1 of the design names (after `x7-gate` and
`sortable-encoding`), X at the `xapiand` branch (Xapiand:e692a64b1 when the design was checked).

## Decisions

- The architect's re-check of revision 1 (L1 to L4, scoped to `33ccf9e3e..HEAD`; `docs/reviews/closing-benchmark-design.md`) is the quest's `design` lens.
- Blocked on the owner: the design publishes from an AWS `c7i.metal-24xl` in `lease run`s (Q7: the
  metal window, `DAILY_USD`, dates agreed with Detent), and the harness was proven on AWS leases
  (`c7a.2xlarge`); the owner's 2026-11-04 host rule is no AWS, and campaign never rents machines.
- Risk low (harness tooling and measurement); the design review is A1's re-check. Measurements are
  `exclusive` jobs of at most 30 minutes, so D14's 45-minute runs are split.
- Adopts `fix/closing-bench-harness` (Xapiand:ec4e6d937) as the quest branch and
  `design/closing-benchmark` (Xapiand:cf75407dc, revision 1 and its re-check) as task `design`;
  both share the design's first two commits.
- Prior evidence: the local baseline of 2026-11-04 (harness Xapiand:ec4e6d937, 200,000 documents,
  every row, five rounds, local QEMU VM, smoke layout): `docs/perf/closing-baseline-2026-11-04/` on
  master. It shows the harness runs end to end without AWS, S behind E or Q on T1, T2,
  T4b and T6 (T4b `agg_histogram` 71.6x), and one REGRESSION against X: T11 idle memory 1.28x
  (A12's row). Smoke values: it decides nothing published.
- Open (owner, PLAN open question 4): a non-AWS host for the published run. The owner's draft
  `~/code/local-benchmarking.md` (2026-11-02: one local Linux x86-64 box with four timing slots,
  nothing bought) is the alternative on record; `~/code/cloud-benchmarking.md` (2026-11-01) is the
  rental plan the 2026-11-04 rule ended.
- Open (lead): the harness ledger's open findings (Quickwit's crash behavior for T7; `lease run`'s
  "unexpected EOF" once) and the `es_loadtest.py` deletion's design-vs-code items 1 to 11.

## May land early

The harness (tools, adapters and their offline tests) and the design, which change no product code.
