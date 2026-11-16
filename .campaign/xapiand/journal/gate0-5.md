# gate0-5: journal

State: landed
Quest: .campaign/xapiand/quests/gate0-5.md (the lead's; never edit it)

## Doing

Nothing: landed at Xapiand:5b865a059 on 2026-10-27 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/gates/phase0.5.md`: "Build works; 339 requests in 339 lines (7 readiness probes, 11 unattributed lines, 0 tainted)" (run 2 T1: 339 requests in 339 lines, 576 of 576 assertions) and `docs/dev/engine-census.md` |
| A2 | `docs/gates/phase0.5.md`: "Exit 0: 7 files, 158 lines in the allowlist" (T2: exit 0, 7 files and 158 lines in the allowlist) and `harness/seam_check.sh` |
| A3 | `docs/gates/phase0.5.md`: "339 requests, ours 576/576, base 574/576; 0 regressions, 0 pre-existing, 2 fixes (Basque)" (run 2 T3: ours 576/576, base 574/576, 0 regressions, 2 fixes; 19 body divergences, all classified) |
| A4 | `docs/gates/phase0.5.md`: "0.996x base by trial mean (4456 vs 4476 docs/s; by run median 0.989x, 4470 vs 4520)" (run 2: bulk index 0.996x base, query QPS 0.974x base, both inside the band) |
| A5 | `docs/gates/phase0.5.md`: "Both GREEN; [3b] every node reports `_doc_count=40 _sum=530 _avg=13.25`" (run 2 T4: cluster 2 and 3 GREEN with [3b]; WAL recovery 50/50, replay and aggregate replay PASS) |
| A6 | `docs/gates/phase0.5.md`: "Census over the suite: 2625 documents written, 2625 with exactly one `Q` term" (run 2 T5: 2,625 documents, each with exactly one Q term; id_terms_check 20 of 20 ranges) |
| A7 | `docs/dev/extraction-ledger.md`: "with the three copies of the version dance collapsed into one Xapian implementation" (rows of 2026-10-20 and 2026-10-21: analysis, word breaks, the engine seam's document and shard halves) |

## Result

Landed at Xapiand:5b865a059 on 2026-10-27. Gate report `docs/gates/phase0.5.md`: run 1 on Xapiand:cca0011bb (merged as Xapiand:79554ef8c) held by the lead's verdict; run 2 on Xapiand:7efc3753e (merged as Xapiand:98e48e4eb), then "Gate 0.5 passes on master 7efc3753e. Every row passes". No architect review was named for this gate; the lead's verdict is the record.

- T3 assertions: 339 requests, 576 of 576 on the seam against 574 of 576 on the base (2 Basque fixes).
- Throughput, four interleaved pairs under quiet exclusive: bulk index 0.996x base (run 1: 0.81x), query QPS 0.974x base.
- Values: numeric_range_check 797 ranges over integer, positive, floating and datetime fields, 0 wrong.
- Open, not gating (the verdict): a RESTORE that stalls about 64 s roughly once in 40 runs on every binary (hunt list); T11 starts with S1's first measurement.
- Docs: `docs/gates/phase0.5.md`, `docs/dev/engine-census.md`, `docs/dev/extraction-ledger.md`, `docs/design/xapiand-on-strata.md`.

## Rulings I made

None beyond the quest file's Decisions (the gate report is the record).
