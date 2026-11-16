# drop-threadpool: journal

State: landed
Quest: .campaign/xapiand/quests/drop-threadpool.md (the lead's; never edit it)

## Doing

Nothing: landed at strata:864ea69 on 2026-11-02 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/drop-threadpool.md`: "Pin moved (this branch's commit)." |

## Result

Landed at strata:864ea69 on 2026-11-02. CI at 3f564dc: run 41546532591, all 8 jobs green. No review named in the ledger.

- Merge: Merge branch 'phase7c/drop-threadpool' (ThreadPool removed; one pool, WorkPool).
- Pin commit replaces threadpool:65bc0bc with eb4751a.
- `preflight.sh` mac hardened, mac ASan+UBSan, Linux, and FreeBSD pass.
- Prospect pin and landing sequence remain next.
- Docs: none.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
