# phase7c-x4: journal

State: landed
Quest: .campaign/xapiand/quests/phase7c-x4.md (the lead's; never edit it)

## Doing

Nothing: landed at Xapiand:10ab134ad on 2026-11-02 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/phase7c-x4.md`: "[x] **K1. Isolation.** `tests/test_lua.cc` `test_interleaved_calls_keep_their_own_doc_and_globals`" |
| A2 | `docs/status/phase7c-x4.md`: "[x] **K2. Runaway scripts.** Unit tests (`test_lua`) and live (`harness/lua_bounds.py`, Release node)" |
| A3 | `docs/status/phase7c-x4.md`: "[x] **K3. Yielding.** `test_a_long_script_lets_other_work_run`" |
| A4 | `docs/status/phase7c-x4.md`: "[x] **K4.** `grep -rn sync_wait src/lua src/script.*` is empty." |
| A5 | `docs/status/phase7c-x4.md`: "Unit tests 29 of 29 on **Release**, **Debug** and **ASan+UBSan** (`detect_leaks=0`), each rebuilt at the head." |

## Result

Landed at Xapiand:10ab134ad on 2026-11-02. Release, Debug, and ASan+UBSan tests: 29 of 29; TSan Lua targets 4 of 4, 0 reports. Review: `docs/reviews/phase7c-x4-security.md` -- REJECT; ledger says four blockers fixed, awaiting security reviewer re-check.

- Merge: Merge branch 'phase7c/x4-lua' (7c X4: Lua as bounded, yielding coroutines).
- Xapiand:22d6ff437, Xapiand:52dfd2040, Xapiand:162d1c369, Xapiand:95cb14ba3, Xapiand:a62c1277a, Xapiand:9729de23e, Xapiand:4700151bf, Xapiand:ee597976d.
- Docs E2E: 641 of 641 assertions (371 requests); fuzz_lua: 6,077 scripts Release and 2,954 ASan/UBSan in 60 s, no violation.
- Lua bounds harness: 30 cases; 10 concurrent swallowers answer 400 and 20 of 20 fresh GETs answered.
- Security fixes: flood probes improved from 697 / 2,125 ms to 5.1 / 13.1 ms for 8 MiB find; final light GET flood p99 57.95 ms.
- Result conversion probes refused oversized cases with 413; peak RSS under 60 MB for listed 1 MiB and 4/8 MiB cases.
- Reviewer re-check with probes remains pending; flag-only test and listed follow-ups remain.
- Docs: `docs/mechanisms/lua-bounds.md`, `docs/dev/patches.md`, `docs/design/phase7c.md`, `docs/src/content/docs/changelog.md`.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
