# phase7c-x4-r1: journal

State: landed
Quest: .campaign/xapiand/quests/phase7c-x4-r1.md (the lead's; never edit it)

## Doing

Nothing: landed at Xapiand:8ef0f8633 on 2026-11-02 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/phase7c-x4-r1.md`: "[x] **R1.** `note()` condemns a call (reason `call`, 400) once the work since the hook last ran passes `max_call_work`" |
| A2 | `docs/status/phase7c-x4-r1.md`: "[x] **R3.** The close-handler shapes raise `error({})`" |
| A3 | `docs/status/phase7c-x4-r1.md`: "[x] **R4, R2.** Changelog and `scripting.md`: `LUAI_MAXSTACK` 200,000" |

## Result

Landed at Xapiand:8ef0f8633 on 2026-11-02. Release ctest 31 of 31; ASan/UBSan ctest 31 of 31; TSan Lua tests 2 of 2, 0 reports. Review: `docs/reviews/phase7c-x4-security.md` -- REJECT then re-check conditions R1-R4 closed; reviewer probes pending.

- Merge: Merge branch 'phase7c/x4-r1' (Lua per-call work cap; X4-R1 to R4).
- Xapiand:dace91053 and Xapiand:7dc39f5fd; ledger commit is not named.
- Docs E2E: 641 of 641 (371 requests); harness: 33 bound cases and 1,436 flood requests answered 400.
- Flood light-GET p99 beside 16 clients: 57.95 ms; configured limit --max-flood-p99-ms 100.
- Hold tests: worst single call at cap 19 to 26 ms; a completing 1 MiB gsub call 31 ms.
- Fuzz under ASan/UBSan: seeds 1 and 2, 30 s each, no violation; seed 7 retains a pre-existing oracle mismatch.
- Reviewer probes ra_stall.py and rc_flood.py against this head remain next.
- Docs: `docs/mechanisms/lua-bounds.md`, `docs/dev/patches.md`, `docs/design/phase7c.md`, `docs/src/content/docs/changelog.md`.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
