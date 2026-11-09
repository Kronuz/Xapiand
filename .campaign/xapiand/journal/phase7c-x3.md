# phase7c-x3: journal

State: landed
Quest: .campaign/xapiand/quests/phase7c-x3.md (the lead's; never edit it)

## Doing

Nothing: landed at Xapiand:d5a2e746c on 2026-11-01 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/phase7c-x3.md`: "[x] **1. Pins and service.** The nine pins; `SearchService::handle` is a coroutine and `should_offload` is gone" |
| A2 | `docs/status/phase7c-x3.md`: "the per-index cap is a `reactor::AdmissionGate` in each `Core` (`view_gate`, limit `readers_per_index`)" |
| A3 | `docs/status/phase7c-x3.md`: "`body_reader` is `body_stream`: `Request::read_chunk` reads it with `task::await_asio`" |
| A4 | `docs/status/phase7c-x3.md`: "[x] **4. Proof** (below). Review fixes below." |

## Result

Landed at Xapiand:d5a2e746c on 2026-11-01. Release, Debug, and ASan/UBSan: 27 of 27 ctest targets; TSan concurrency 9 of 9, 0 reports. Review: `docs/reviews/phase7c-x3-code.md` -- APPROVE WITH CONDITIONS.

- Merge: Merge branch 'phase7c/x3-views' (7c X3: requests as coroutines on the reactors).
- Xapiand:b4915e5dd, Xapiand:b7b1361b9, Xapiand:df93cf455, Xapiand:21012b19f, Xapiand:a89f6b0d4.
- Docs E2E: 641 of 641 assertions (371 requests); WAL recovery PASS (50 of 50).
- Crash check: A 50 of 50; B 3,723 acknowledged and 3,723 recovered; D 2 writers and 6,714 recovered, each a prefix.
- Stored upload tests: 33,554,432 bytes, 16,777,217, and 16,773,120 read back byte-equal; four 20 MiB uploads byte-equal.
- Restore stream check: 20,000 NDJSON documents and 5,000-document MsgPack stream counted back; interrupted stream preserved 13,076 of 20,000.
- T2 streaming, RESTORE, and client-disconnect checks pass; large blob streaming out and orphan-volume reclaim remain open.
- Docs: `docs/mechanisms/reader-views.md`, `docs/mechanisms/requests-on-the-reactor.md`, `docs/design/phase7c.md`.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
