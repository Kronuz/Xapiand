# phase7c-x6: journal

State: landed
Quest: .campaign/xapiand/quests/phase7c-x6.md (the lead's; never edit it)

## Doing

Nothing: landed at Xapiand:2ff594ebe on 2026-11-02 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/phase7c-x6.md`: "[x] **K1. Deletions (D4, D13, D15).** `Xapiand:432f9a45f`." |
| A2 | `docs/status/phase7c-x6.md`: "[x] **K2. Destructor asserts (D14).** `Xapiand:0cc864607`, `Xapiand:640a6bf84`." |
| A3 | `docs/status/phase7c-x6.md`: "[x] **K3. Shutdown (D16).** `Xapiand:402a03ce5`, `Xapiand:3dd4dd5f1`." |
| A4 | `docs/status/phase7c-x6.md`: "[x] **K4. Enforcement in CI.** `Xapiand:3189ab438`." |
| A5 | `docs/status/phase7c-x6.md`: "[x] **K5. Docs.** `Xapiand:fe13fc906`." |
| A6 | `docs/status/phase7c-x6.md`: "[x] **K6. Proof.** At `Xapiand:fe13fc906` plus the test commit `Xapiand:3dd4dd5f1`" |

## Result

Landed at Xapiand:2ff594ebe on 2026-11-02. Release, Debug, ASan/UBSan ctest: 29 of 29; docs E2E 641 of 641 assertions (371 requests). No review named in the ledger.

- Merge: Merge branch 'phase7c/x6-deletions' (7c X6: offload pools and thread options removed; teardown asserts; one stop bound; async_check in CI).
- Xapiand:432f9a45f, Xapiand:0cc864607, Xapiand:640a6bf84, Xapiand:402a03ce5, Xapiand:3189ab438, Xapiand:fe13fc906, Xapiand:3dd4dd5f1.
- TSan concurrency targets report 0; mixed load: 17,148 documents, final dump equals model, 0 failures, 0 sanitizer reports.
- Strict gate: 264 hits, allow 219, owed 31, compiled out 13, 0 unclassified; planted sync_wait exits 1.
- Thread census: total 51, offenders none; 16 HTTP loops on the 16-CPU macOS run.
- One shutdown bound is 35 s; tests with a 2 s bound exercise abandonment and second-signal exit 75.
- CI gate's own tests: 14 of 14; --strict --owed remains open pending X1, X4, and owed-row review.
- Docs: ARCHITECTURE.md, `docs/src/content/docs/architecture/internals.md`, `docs/mechanisms/control-plane.md`, `docs/src/content/docs/options.md`, `docs/src/content/docs/changelog.md`.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
