# document-gate: The document profile gate, strata's core against RocksDB

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status document-gate`.

## Outcome

strata's lifecycle core, used as a key-value engine, matches or beats RocksDB on all four axes:
throughput and latency percentiles, memory, disk and write amplification, and CPU per operation, on
db_bench's workloads and YCSB A to F, with data that fits, 2x and 10x an equal memory limit, under each
durability tier. Its prerequisites are in: standalone durability through the turn log, format 13's
ordered `keys` section, and snapshot publication. The core work toward it landed under this milestone
(format 14's key-value layout, scans, the write buffer, shared readers, flush and merge memory,
retention and reclamation); this quest is the remaining prerequisite and the gate itself.

## Acceptance criteria

From strata `docs/PLAN.md`, "The `document` profile gate (after Phase 4)", and strata
`docs/design/rocksdb-survey.md` (owner decisions; K2).

- A1 Standalone durability through the turn log: a standalone index uses Track C D11's turn log as a group of one, with the manifest log as its checkpoint (`manifest-log`; rocksdb-survey K2; strata `docs/design/manifest-log.md` D9 and Q1): tests in the build-test check
- A2 [artifact] Format 13's ordered `keys` section and snapshot publication are in place (`format13`, `shared-reader-views`, `write-buffer-reads`), cited at the gate's commit: `docs/gates/document-profile.md`
- A3 Gate: the key-value ratchet family on db_bench's workloads (fillseq, fillrandom, readrandom, seekrandom, ...) and YCSB A to F, at data that fits, 2x and 10x an equal memory limit, under each durability tier: exclusive jobs
- A4 Gate: all four axes per row: throughput and latency percentiles, memory, disk and write amplification, CPU per operation; strata at most 1.0x RocksDB's time and at least 1.0x its throughput (the Phase 6 margins after): exclusive jobs
- A5 Gate: RocksDB pinned, on its fastest configuration per value-size class, with the harness's fairness audit applied (strata `docs/perf/rocksdb-fairness-audit.md`): jobs
- A6 Gate: on Linux and FreeBSD, and on arm64 before Phase 3R's next gate: jobs on the Linux VM, the FreeBSD VM and an arm64 host
- A7 Every gate: ASan, UBSan, TSan; one CPU-hour of fuzzing per decoder added or changed (format 14, the manifest log); readers reject unknown versions; fixtures; coverage; the scale statement: jobs
- A8 [artifact] The gate report, every row with its commit, platform and jobs: `docs/gates/document-profile.md`

## Contracts

Consumes the manifest log (`manifest-log`, strata-manifest-log) and the turn log of Kronuz/journal.

## Decisions

- The lead's priority (owner, 2026-10-17): strata is a lifecycle core that must beat RocksDB and search
  plugins that must beat Lucene and Tantivy; the core against RocksDB is calibrated once (db_bench
  fillseq, fillrandom, readrandom, seekrandom, YCSB A to F at about 1M keys), then timed against
  RocksDB only (strata `docs/status/README.md`, "Priority").
- The write buffer first (owner, 2026-10-22); the lanebuf scan tail accepted at 1.5 to 1.8x RocksDB's
  scan p99.9 at full write speed (owner, 2026-10-26; `lanebuf`).
- Prior evidence, not to redo: strata `docs/perf/core-vs-rocksdb.md` and its `-0021`, `-1022` and `-f14`
  runs, `docs/perf/write-buffer-bakeoff.md`, `docs/perf/lanebuf-scan-tail.md`, `docs/research/no-stalls.md`.
- Ordering: lands after `manifest-log` (A1) and `phase4-gate` (the gate follows Phase 4).
- The arm64 host for A6: none beyond GitHub's CI (D36), so its arm64 rows wait for one.
- Follows the release (D39); the fuzzing of the formats the release writes is `format-freeze`'s.

## May land early

The harness and report changes.
