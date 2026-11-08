# lanebuf: journal

State: landed
Quest: .campaign/xapiand/quests/lanebuf.md (the lead's; never edit it)

## Doing

Nothing: landed at strata:18febff on 2026-10-30 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/lanebuf.md`: "`a4aa2c4` rebuild (hazard registry, position index, fixed chunk directory, scan-triggered builds" |
| A2 | `docs/status/lanebuf.md`: "runner, adapters (RocksDB, main), bytes mode, prebuilt RocksDB clones, JSON fix, E1 behind `LANEBUF_E`." |
| A3 | `docs/status/lanebuf.md`: "Results and verdicts: `docs/perf/write-buffer-bakeoff.md`; raw lines: `docs/perf/write-buffer-bakeoff-data/`." |

## Result

Landed at strata:18febff on 2026-10-30. At full write speed, p99.9 improves from 4 ms to 40 to 63 us; p99 is 31 to 41 us. No review named in the ledger.

- Merge: Land the documents of the unmerged branches, labelled, and triage the branches.
- Seven scan-tail experiments were run; T11a/T11b and tuning were not run.
- At 2 MB/s, p99.9 is 7 us, 0.33 to 0.39x RocksDB's.
- Memory is +8.3% per record and +528 B per index after one write; freeze is -9%.
- T1 is -1 to -3% (inside A/A); T2 is unchanged.
- Other platforms and T4 remain open; clean T11 measurements are blocked on quota.
- Docs: `docs/perf/write-buffer-bakeoff.md`, `docs/perf/lanebuf-scan-tail.md`, docs/perf/write-buffer-bakeoff-data/.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
