# phase7c-linux-gate-fixes: Repair Linux gate failures

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at Xapiand:ebfb8fdfe (2026-11-02); its ledger `docs/status/phase7c-linux-gate-fixes.md` is the record. Its machine state is in `state.json`: `campaign status phase7c-linux-gate-fixes`.

## Outcome

The BlockingScheduler signal race is fixed, and `msgpack_load` refuses body-driven excessive allocations. Compiler compatibility, harness, missing target and GCC build failures found by the dry run are addressed.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] Six Linux dry-run findings are closed or confirmed already fixed on master: `docs/status/phase7c-linux-gate-fixes.md`
- A2 [artifact] CI-1: GCC 13 ICE fixed: `docs/status/phase7c-linux-gate-fixes.md`
- A3 [artifact] CI-2: FreeBSD format failure identified as the stringified guard issue: `docs/status/phase7c-linux-gate-fixes.md`
- A4 [artifact] `fuzz_body.cc` GCC preprocessing failure fixed: `docs/status/phase7c-linux-gate-fixes.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Keep the libstdc++ fallback unless libc++ advertises `__cpp_lib_format`.
- Pin RapidJSON at `24b5e7a`, matching msgpack's pin.
- Risk high: Bounds untrusted MessagePack bodies and fixes scheduler concurrency (the lead's mapping).

## May land early

None.
