# phase7c-http-wave: journal

State: landed
Quest: .campaign/xapiand/quests/phase7c-http-wave.md (the lead's; never edit it)

## Doing

Nothing: landed at Xapiand:0f01d54dc on 2026-11-03 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/phase7c-http-wave.md`: "[x] 1 `AsioHost` wraps `reactor(i).scheduler()` and the control reactor's `scheduler()`" |
| A2 | `docs/status/phase7c-http-wave.md`: "[x] 6 `take_turn`'s deadline linked to the request's token (F2), through `Shard::cancel`" |
| A3 | `docs/status/phase7c-http-wave.md`: "[x] 8 `Run` and `Run::await` deleted with F1's `Run::Resume` wrapper" |
| A4 | `docs/status/phase7c-http-wave.md`: "[x] 9 reactor Session migration: Xapiand builds no session of its own" |
| A5 | `docs/status/phase7c-http-wave.md`: "[x] H1: `SearchService::handle` returns `task::task<void>`, views awaited" |
| A6 | `docs/status/phase7c-http-wave.md`: "[x] 7 again through `HttpAsioService`'s phases" |
| A7 | `docs/status/phase7c-http-wave.md`: "[x] Floor: Linux TSan and ASan+UBSan, macOS ASan; graceful_shutdown_check, docs E2E" |
| A8 | `docs/status/phase7c-http-wave.md`: "[x] H2: `Request::read_chunk` awaits `BodyStream::read(chunk, http_req->cancel_token())`" |
| A9 | `docs/status/phase7c-http-wave.md`: "[x] W2 closed: http:73cfa02 checks affinity against the loop thread" |
| A10 | `docs/status/phase7c-http-wave.md`: "[x] W3: `XapiandManager::http_shutdown_grace(left) = max(250 ms, (left - 5 s) / 2)`" |
| A11 | `docs/status/phase7c-http-wave.md`: "The test now paces the keys (50 every 20 ms, all 5,000 pending when the first fires)" |

## Result

Landed at Xapiand:0f01d54dc on 2026-11-03. macOS ASan request/control/host/node tests pass; docs E2E 644 of 644 assertions, 374 requests. No review named in the ledger.

- Merge: Merge branch 'phase7c/http-wave' (reactor R, http H1 and H2 on task; the HTTP grace from D16's budget).
- `Xapiand:f29690a3a`, `bb6fa6e34`, `42d5d4f9b`, `d2e6901c3`, `44a5ff985`, `6408e2200`, `1e9b90042`, `c3af6d28a`, `927acf570`, `22ed28a80`, `7b1dcb563`, `095402c97`, `376eebf23` recorded.
- Linux Release, ASan+UBSan and TSan ctest: 35 of 35 each.
- Lua flood p99 93 ms passes its 100 ms limit on one run; 116 and 180 ms on branch and 129, 112 and 127 ms on master fail under shared-VM contention.
- `stall_load` completes with 0 errors and 0 503s; async check 0 owed, 0 unclassified.
- Schema-load contention flake remains open; shared-VM Lua timing failures are also noted.
- Docs: `docs/mechanisms/control-plane.md`.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
