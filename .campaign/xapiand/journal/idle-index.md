# idle-index: journal

State: landed
Quest: .campaign/xapiand/quests/idle-index.md (the lead's; never edit it)

## Doing

Nothing: landed at strata:1a04bc3 on 2026-10-27 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/idle-index.md`: "Harness `idle_index` (build/open, bare and shared, idle window, query, close) and `idle_index_count`" |
| A2 | `docs/status/idle-index.md`: "macOS: 0/1/10/100/1,000 documents at 1k, 10k, 100k open; 126,507 (vnode ceiling) and 252,492 (0 docs)." |
| A3 | `docs/status/idle-index.md`: "Linux VM (port 18772): same matrix with cold and warm opens; 300,000 open" |
| A4 | `docs/status/idle-index.md`: "Heap breakdown per idle index; limits table; top three cuts; comparison with the field." |

## Result

Landed at strata:1a04bc3 on 2026-10-27. macOS reaches 126,507 indexes at the vnode ceiling and 252,492 with 0 docs. No review named in the ledger.

- Merge: Merge branch 'perf/idle-index'.
- `docs/perf/idle-index.md` and `bench/idle_index/idle_index.cc` deliver the report and harness.
- Linux VM reaches 300,000 open; `vm.max_map_count` is 65,491.
- Engine code is unchanged.
- Next: A/B cold-segment cut; other cuts and platforms remain open.
- Docs: `docs/perf/idle-index.md`.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
