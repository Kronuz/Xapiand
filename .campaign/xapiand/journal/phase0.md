# phase0: journal

State: landed
Quest: .campaign/xapiand/quests/phase0.md (the lead's; never edit it)

## Doing

Nothing: landed at strata:a2d79a4a on 2026-10-11 under the earlier campaign skill.

## Next

1. Nothing: landed. What stays open is named under Result.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `tools/vm/linux.sh`: "strata's Linux gate VM: Ubuntu 24.04 under QEMU with HVF acceleration." (in the plan's first commit, strata:665677b1) |
| A2 | `tools/vm/freebsd.sh`: "strata's FreeBSD VM: FreeBSD 14.4 (UFS) under QEMU with HVF acceleration." (on main by strata:2a111373, 2026-10-14, after the gate) |
| A3 | open: no benchkit library or directory on strata or prospect main (2026-11-08); interleaved trials run through prospect's bench/phase2/gate.py and tools/vm/quiet_trials.sh |
| A4 | open: `bench/phase0/README.md` records the three engines on Wikipedia (5,032,104 documents) and Xapiand on 100,000 Wikipedia documents; no Xapiand-shaped corpus run |
| A5 | open: `bench/phase0/README.md` drives Xapiand with eight clients; no Elasticsearch system-level run at Phase 0 (Xapiand's loadtest.py gained an Elasticsearch system for the closing benchmark, strata:27699282, 2026-11-03) |
| A6 | open: `bench/phase0/README.md` profiles the RESTORE path (perf record, 25,572 samples); no query-path profile is recorded |
| A7 | `bench/phase0/README.md`: "The hop costs about 350 µs per search, 15% of its latency." ("Offload hop": 2,375 against 2,028 µs mean, about 350 µs, 15%) |
| A8 | `bench/phase0/README.md`: "The Phase 6 baseline is the offloaded row: light-query p99 of 7.2 ms under heavy load." ("Light queries under heavy load": p99 7,219 µs offloaded, 3,020 µs inline) |
| A9 | `bench/phase0/README.md`: "4.1 ms per 4 KiB `O_DSYNC` write (2,000 writes)" ("Environment": QEMU 9 with HVF, 8 vCPUs, 16 GB; 4.1 ms per 4 KiB O_DSYNC write) |
| A10 | `bench/phase0/postings.cc`, and `docs/gates/phase-1.md`: "Lucene's `.doc` with skip and impact data: 990.6 MB" (bitpack size row: Lucene's .doc 990.6 MB) |
| A11 | `docs/reviews/phase-1.md`: "The rank vectors cover 300 queries over 10,000 documents with 17-digit weights, percents, and bounds" ("Phase 0 additions" 3: rank vectors over 300 queries and 10,000 documents; term-generation vectors for English stemming strategies and CJK n-grams); the harness, strata-oracle, is GPL and kept outside git |
| A12 | `bench/phase0/query.json` |
| A13 | `docs/FEASIBILITY.md`: "of 100,000 Wikipedia documents runs at 964 docs/s, and Xapian accounts for 28.2% of it" ("Phase 0 measures where the time goes": RESTORE 964 docs/s, Xapian 28.2%) |
| A14 | `bench/phase0/README.md`: "The hop costs about 350 µs per search, 15% of its latency." ("Offload hop") |

## Result

Landed at strata:a2d79a4a on 2026-10-11: Phase 0's gate closes when FEASIBILITY records the RESTORE attribution (`docs/reviews/phase-1.md`, "Phase 0 additions" 1). Review: `docs/reviews/stage-1.md` (stage 1, APPROVE WITH REQUIRED CHANGES, approved for Phase 0 and Phase 1) and `docs/reviews/phase-1.md` (its Phase 0 rulings).

- Indexing, 5,032,104 Wikipedia documents, one merged index: Tantivy 0.26 14,033 docs/s (4 threads), Lucene 10.4.0 14,135, Xapian 2.1.0 881 (1 thread, 1:35:09, 14.38 GiB on disk).
- Queries, 962 harness queries, fastest of 10 runs: Xapian 6 to 19 times slower than Lucene and Tantivy on every command; Lucene's counts equal Tantivy's on every query.
- Xapiand RESTORE of 100,000 documents: 964 docs/s; Xapian `replace_document` 28.2%, per-document check-in 25.4%, WAL deletion enqueues 14.6%, document preparation 10.2%, checkout 7.7%. An infinitely fast engine bounds the RESTORE speedup at 1.39x; the two host fixes went to Xapiand's track.
- Offload hop about 350 µs per search (15%); light-query p99 under heavy load 7.2 ms offloaded, 3.0 ms inline, with 31% fewer heavy queries inline.
- Provisional: every row ran on a shared host; the Phase 2 gate re-ran the engine rows interleaved on a quiet host. The hop and the light-query p99 are VM numbers owed on bare metal (the review); Phase 7c's own baseline (S1, Xapiand `docs/design/phase7c.md` T5) replaced the light-query baseline.
- Docs: `docs/FEASIBILITY.md`; prospect `bench/phase0/README.md`.
- Left open: A3, A4, A5, A6.

## Rulings I made

None beyond the quest file's Decisions (the plan and the reviews are the record).
