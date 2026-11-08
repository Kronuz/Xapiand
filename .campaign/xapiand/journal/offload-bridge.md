# offload-bridge: journal

State: landed
Quest: .campaign/xapiand/quests/offload-bridge.md (the lead's; never edit it)

## Doing

Nothing: landed at Xapiand:44f628903 on 2026-10-26 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/offload-bridge.md`: "[x] C1 The rule worded for D13 (above). The lead applies it in strata (outside this branch)." |
| A2 | `docs/status/offload-bridge.md`: "[x] C2 One bridge, one guard: `git grep -n 'task::sync_wait' src` lists only `src/engine/sync.h`;" |
| A3 | `docs/status/offload-bridge.md`: "[x] C3 FLUSH offloaded (`search_service.cc`), both comments corrected; changelog bullet." |
| A4 | `docs/status/offload-bridge.md`: "[x] C4 The restore view finishes and drops `request.indexer` on its worker on every exit." |
| A5 | `docs/status/offload-bridge.md`: "[x] C5 The control-plane reactor has one worker: setup_node's database part, the dispatched" |
| A6 | `docs/status/offload-bridge.md`: "[x] C6 `Offload`, `set_offload`, `XAPIAND_ENGINE_OFFLOAD_THREADS` removed; `Hop` takes a" |
| A7 | `docs/status/offload-bridge.md`: "[x] The guard fires in `test_blocking_guard` for each site on a reactor loop and on a scheduler" |
| A8 | `docs/status/offload-bridge.md`: "[x] Docs E2E GREEN/GREEN, 0 ASan reports, 0 guard hits, clean exit." |
| A9 | `docs/status/offload-bridge.md`: "[x] Hammer (`stallcount.sh 0`, 5 rounds of 16 clients with FLUSH) 0 timeouts, no 5xx; `uafhunt.sh`" |
| A10 | `docs/status/offload-bridge.md`: "[x] TSan (Apple clang 17): `test_pool` and `test_blocking_guard`, 3 runs each, 0 warnings." |
| A11 | `docs/status/offload-bridge.md`: "[x] Q1: no separate cap for Xapian I/O." |

## Result

Landed at Xapiand:44f628903 on 2026-10-26. E2E: 339 requests, 576/576 assertions; 0 ASan reports and 0 guard hits. Reviews: `docs/reviews/offload-bridge-code.md` -- approve with conditions; the architect's ruling on the offload bridge (engine-core CR1, CR2, pool CR5): `docs/reviews/engine-offload-architect.md` -- APPROVE WITH CONDITIONS.

- Merge: Merge branch 'fix/offload-bridge'.
- Xapiand:e355c62d8 -- bridge and guard; 6e0a7e092 -- FLUSH; 1388f4175 -- restore indexer.
- Xapiand:288ad8010 -- control-plane worker; d49a1d0b8 -- retire `Offload`; later commits move shutdown, DB_UPDATED, and replication teardown.
- Reactor:d381e4d -- `inside_run()` marker; reactor:0c04d0d -- UDP marker test.
- FLUSH hammer: 5 rounds of 16 clients, 0 timeouts, no 5xx; UAF hunt 10 rounds, 0 reports.
- TSan: 3 runs each of `test_pool` and `test_blocking_guard`, 0 warnings.
- Open: Linux, FreeBSD, and timing not run; remote-protocol teardown remains unhandled by the rule.
- Docs: none.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
