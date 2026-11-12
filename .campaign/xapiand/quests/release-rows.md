# release-rows: The rows of Phases 3 to 6 the release waits on

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status release-rows`.

## Outcome

Before the first Xapian-free release, the rows of Phases 3 to 5 that guard what users get back pass
at the prospect and strata commits the release pins (D39): merging leaves every index answering as
before, deletes included; every operator Xapiand serves returns what Xapian returns on the oracle's
vectors; every aggregation gives the same answer over one shard and over many; and every stemmer
Xapiand's schema accepts produces Xapian's vocabulary. prospect's decoders get the fuzzing the
freeze-readiness review needs (`format-freeze`). Phase 6's latency rows are 7c's T4 and T5
(`x7-gate`). The phase gates keep these rows and re-run them at their own commits after the release.
Repository: Kronuz/prospect, which holds the search engine and the harness.

## Acceptance criteria

From strata `docs/PLAN.md` (Phases 3 to 5, "Every gate") and D39. Each row runs at the release's
pins, named in the report.

- A1 `verify` and `parity` on multi-segment indexes before and after merging, `parity` with deletes
  included (`phase3-ratchets` A6): jobs
- A2 Operator parity against the oracle's vectors for every operator Xapiand serves (`phase4-gate`
  A8): jobs
- A3 Aggregations equal over one shard and over many for every collector Xapiand serves
  (`phase4-gate` A9; strata `docs/DISTRIBUTION.md`, "Query"): jobs
- A4 Stemmer vocabularies equal to Xapian's for every language Xapiand's schema accepts (`phase5` A6;
  the oracle's vectors): jobs
- A5 Every decoder of prospect's formats that the release reads: one CPU-hour of fuzzing with zero
  findings; readers reject unknown versions, with a fixture per format: jobs
- A6 The tests behind A1 to A5 pass under ASan and UBSan: the `asan` check
- A7 [artifact] The report: each row with its commit, the release's prospect and strata pins, and its
  jobs; each difference from the oracle fixed, or listed as a behavior change with who decided it:
  `docs/gates/release-rows.md`

## Contracts

None: measurement. A row that fails is fixed by its own quest and measured again.

## Decisions

- Lead, 11-11, on review F1: A3 is a MISS at the release's level (Xapiand master serves
  aggregations from its own src/aggregations, not prospect's collectors); prospect's 16-kind test
  stays as prospect's evidence; the release's row is hunt row 91 (Xapiand shard-parity test).
- Lead, 11-11, on review F2: A5 is a MISS as "one CPU-hour": six independent 10-minute starts per
  decoder from one seed corpus, wall-clock at nice 19, coverage still rising; the report states the
  reviewer's numbers; follow-ups are hunt row 92 (slow targets) and keeping the corpus across runs.
- D39: these rows gate the release; the gates of Phases 3, 4 and 5 keep them and re-run them at
  their own commits after it.
- The oracle is Kronuz/strata-oracle (GPL-2.0, kept apart so prospect and strata stay MIT): its
  vectors are data the tests read; its code never enters prospect.
- `_terms` counts past `_shard_size` are approximate by design, as Elasticsearch's are
  (behavior-changes audit #25); A3's report states how the row treats them.
- If the pins move before the release, `release` re-runs these rows at the new pins.

## May land early

The harness and report changes.
