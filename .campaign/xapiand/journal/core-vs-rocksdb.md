# core-vs-rocksdb: journal

State: landed
Quest: .campaign/xapiand/quests/core-vs-rocksdb.md (the lead's; never edit it)

## Doing

Nothing: landed at strata:b8a2cb6 on 2026-10-20 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/core-vs-rocksdb.md`: "881fc41: plan and harness (`bench/kv/kvgen.hh`, `kvdriver.hh`, `kv_strata.cc`, `run.py`;" |
| A2 | `docs/status/core-vs-rocksdb.md`: "c16f57e: `search_layout` fallback (`basic_index<document_layout>` does not compile on main)" |
| A3 | `docs/status/core-vs-rocksdb.md`: "the rows the unheld disk cleanup overlapped (fits1, A/A, twox1) rerun and quoted from the reruns" |
| A4 | `docs/status/core-vs-rocksdb.md`: "This commit: results, raw data, the perf index row, this ledger." |

## Result

Landed at strata:b8a2cb6 on 2026-10-20. No review named in the ledger.

- Merge: Merge remote-tracking branch 'origin/perf/core-vs-rocksdb'.
- strata:881fc41 adds the plan and harness.
- strata:c16f57e adds the search_layout fallback, section bytes, and capped runs.
- strata:e671bea adds labeled reruns and heap pause.
- All rows measured on the macOS host; overlapped rows rerun and quoted from reruns.
- The top three losses were profiled as inputs to the core KV design.
- Calibration, A/A, and full sets plus Linux and FreeBSD gate-tier rows remain open.
- deleterandom, multireadrandom, readwhilewriting, and jemalloc (Q5) were not run.
- Docs: `docs/perf/core-vs-rocksdb.md`, docs/perf/core-vs-rocksdb/.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
