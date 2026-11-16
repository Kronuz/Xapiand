# phase7c-x4: Bounded yielding Lua execution

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at Xapiand:10ab134ad (2026-11-02); its ledger `docs/status/phase7c-x4.md` is the record. Its machine state is in `state.json`: `campaign status phase7c-x4`.

## Outcome

Lua scripts run in isolated coroutine environments and yield between budgeted slices. Instruction, memory, matcher, conversion, and result limits prevent the reviewed runaway cases; security re-check remains pending.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] K1 Isolate calls, globals, library tables, and script environments: `docs/status/phase7c-x4.md`
- A2 [artifact] K2 Bound runaway scripts by instruction, non-yieldable, matcher, and memory limits: `docs/status/phase7c-x4.md`
- A3 [artifact] K3 Yield long scripts so other work runs: `docs/status/phase7c-x4.md`
- A4 [artifact] K4 Remove sync_wait from Lua/script paths and keep async_check strict: `docs/status/phase7c-x4.md`
- A5 [artifact] K5 Prove tests, E2E, fuzzing, harness, and mutants: `docs/status/phase7c-x4.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Each call uses its own environment over a shared base; per-call library proxies prevent writes leaking across calls.
- Bound Lua calls return 400; oversized converted results return 413.
- State live-byte cap is shared by interleaved calls of one script; a hog can starve a sibling call.
- Risk high: script isolation and security limits (the lead's mapping).

## May land early

None.
