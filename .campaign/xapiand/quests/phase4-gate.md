# phase4-gate: Phase 4's remaining items and its gate

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status phase4-gate`.

## Outcome

Phase 4, structured data, is complete and gated: ordered numeric keys, the slot table and doc values
by slot, points (`bkd`) and a geo index chosen by measurement, scored filters, computed sort keys,
collapse, exhaustive mode, estimate triples, `_percent` and delete by query, with aggregation
collectors exact across shards, in format 13 with the key dictionary as its own section, optional
sections per segment and real-time get. The filtered, sort, range, geo and aggregation classes beat
Xapian and reach 1.0x of the faster of Lucene and Tantivy, and Phase 3's ratchet rows pass. Format 13,
the layers split and the repository split landed (this milestone's history quests); this quest is what
remains and the gate. Repository: Kronuz/prospect, which holds the search plugins and the harness.

## Acceptance criteria

From strata `docs/PLAN.md`, "Phase 4: structured data" (items and gate) and "Ratchets" (Phase 4 rows),
and prospect `docs/design/phase-4.md`. Each built item is shown by its tests at the gate's commit.

- A1 Ordered numeric keys (a fixed-width map in the format code, not a library), the slot table and doc values by slot: tests in the build-test check
- A2 `bkd` (1-D, 2-D interval, and what geo keeps) and a geo index chosen by measurement; format 13 reserves the points section kind (SYS-4): tests, and the measurement that chose it in jobs
- A3 Scored filters, computed sort keys, collapse, exhaustive mode, estimate triples, `_percent`, delete by query: tests in the build-test check
- A4 Aggregation collectors (Xapiand's metrics and buckets, nested, and term-list based `_terms` as R-P2 leaves it), exact across shards: tests in the build-test check
- A5 Workload-profile items 1 to 3 (strata `docs/design/workload-profiles.md`): the key dictionary as its own section, optional sections per segment, and real-time get, in format 13: tests in the build-test check
- A6 Gate: Phase 3's ratchet rows pass, every carried row at its original target (`phase3-ratchets`): gate VM jobs
- A7 Gate: the filtered, sort, range, geo and aggregation classes win against Xapian and reach at most 1.0x the faster of Lucene and Tantivy, PISA reported beside every row: gate VM jobs
- A8 Gate: operator parity against the oracle vectors: jobs
- A9 Gate: aggregations equal across one shard and many (strata `docs/DISTRIBUTION.md`, "Query"): jobs
- A10 Every gate: ASan, UBSan, TSan; one CPU-hour of fuzzing for each decoder added or changed in the phase (format 13's sections), zero findings; readers reject unknown versions; a fixture per format change; coverage and the phase's test-parity rows; the scale statement: jobs
- A11 [artifact] The gate report, every row with its commit and jobs: `docs/gates/phase-4.md`

## Contracts

The format-13 sections it fills (doc values, points, optional sections) are prospect's segment format:
pre-freeze until the freeze-readiness review before the first release (D30).

## Decisions

- Ordering: lands after `phase3-ratchets` (its rows gate this phase).
- Prior evidence, not to redo: format 13 (`format13`, prospect `docs/perf/format13-landing.md`,
  `format13-gate-vm.md`); sort keys and collapse (D8, D9, prospect:39aae28); aggregation collectors
  with exact, mergeable partials (prospect:0ac8175); the values plugin, opaque bytes by slot
  (prospect:ca67b32, 7a's P1); multi-index search with one global snapshot and the exhaustive collector
  (prospect:40fd4bf, 7a's P4); the Xapian operators (7a's P3, prospect `docs/status/xapian-ops.md`).
- R-P2 (owner, 2026-10-28, D7): no term lists in strata or prospect; P2 (`phase4/term-lists`) is
  abandoned, so `_terms` reads stored values.
- Follows the release (D39), except A8 and A9, which `release-rows` proves before it; this gate
  re-runs them at its own commit.

- Lead, 11-11 (inventory, journal Xapiand:9c665d06a): no split into seven quests. Phase 4 follows the release (D39), so this quest lands its built items as increments (`--upto`, "May land early") and parks after them; release-rows (before the release) goes first. The slot table uses metadata entries (no strata change). `_percent` follows Xapian's documented rule, written from its public documentation and the oracle vectors, never its source. The typed-values note (prospect:b51e0fb) gets its design review when this quest resumes. Delete by query and get by docid need strata core: their own quests when this resumes.
## May land early

Each item alone with its tests; the gate report last.
