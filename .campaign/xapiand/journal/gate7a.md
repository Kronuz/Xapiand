# gate7a: journal

State: landed
Quest: .campaign/xapiand/quests/gate7a.md (the lead's; never edit it)

## Doing

Nothing: landed at Xapiand:0d194335b on 2026-10-29 under the earlier campaign skill.

## Next

1. Nothing: landed. Its carried every-gate items are in `phase7b` and `x7-gate`.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/gates/phase7a.md`: "Xapian 566/566, strata 566/566 (335 requests, 0 uncovered); 0 regressions" (Rerun on Xapiand:8f6332a01: Xapian 566/566, strata 566/566, 335 requests, 0 regressions, 0 status differences) |
| A2 | `docs/gates/phase7a.md`: "5 of 335 bodies differ (listed below), all justified." (Rerun T7: 5 of 335 bodies differ, all justified; hit order, scores, total and count identical on all 92 responses with hits) |
| A3 | `docs/gates/phase7a.md`: "Xapian 6766 of 6766 acknowledged writes recovered, strata 7344 of 7344; none lost, none duplicated" (Closing run on Xapiand:7125dfc28: Xapian 6,766 of 6,766 acknowledged writes, strata 7,344 of 7,344; index check passes on 5 of 5 shards) |
| A4 | `docs/gates/phase7a.md`: "bank, 1000 documents. Xapian to strata: the two dumps are identical." (T9: bank, 1,000 documents, identical both ways after the C4 fix) |
| A5 | `docs/gates/phase7a.md`: "0 ThreadSanitizer reports; the final `DUMP` equals the model (26859 documents)" ("Every-gate checks": TSan mixed loads of 611 s and 307 s, 0 reports; final dump equals the model) |
| A6 | `docs/gates/phase7a.md`: "0.387x (0.1455 against 0.3761 ms; pairs 0.348 to 0.437, 10 of 10 at or under 0.5" (Closing run T11: CPU 0.387x on the gate's estimator, 10 of 10 pairs at or under 0.5; corrected for device-flush wait, 0.357x) |
| A7 | `docs/gates/phase7a.md`: "28 of 28 pass (104 s), 0 reports" ("Every-gate checks": ctest 28 of 28 under TSan and under ASan+UBSan with RapidJSON patched; fuzz one CPU-hour each for four targets; the engine reader rejects an unknown serial version; coverage 91.6% lines, 89.1% branches) |
| A8 | open: `docs/gates/phase7a.md` ("Not run here, with reason, owner and phase": test-parity rows and the scale statement, uncovered-line reasons, Linux, FreeBSD and macOS arm64 sanitizer runs, the fuzz regression corpus, to the 7b gate or CI) |
| A9 | `docs/gates/phase7a.md` (re-check notes closed); strata docs/reviews/phase-7a-gate.md: "L1, L2, C1, C2, C3, C4 closed. No new blocking finding" |

## Result

Landed at Xapiand:0d194335b on 2026-10-29. Gate report `docs/gates/phase7a.md`: first run on Xapiand:9fa890ecc, the Rerun on Xapiand:8f6332a01 (merged as Xapiand:ffcb127d1), the Closing run on Xapiand:7125dfc28 (merged as Xapiand:3bc11f31f), and the every-gate checks of `gate7a-l2`. Review: strata `docs/reviews/phase-7a-gate.md` at Xapiand:3bc11f31f, REJECT on F3 and F6, then the re-check: L1, L2, C1 to C4 closed. strata's `docs/PLAN.md` records "Phase 7a passed (2026-10-29)" (strata:762e03b6).

- T6: 566 of 566 on both engines over 335 requests; after `gate7a-l1` removed the term assertions, 649 of 649 on both over 377 requests (the re-check).
- T8: no acknowledged write lost or duplicated after kill -9 on either engine.
- T11: strata's engine share per document 0.387x Xapian's CPU on the gate's estimator (0.357x without device-flush wait), critical path at most 0.213x (Rerun).
- Every gate: TSan clean over 611 s and 307 s mixed loads; one CPU-hour each for fuzz_strata_document (65.7 million executions), fuzz_strata_pattern (13.2 million), xapiand_fuzz_text (seed 8 found a 9-byte text that never finished, fixed) and xapiand_fuzz_body; coverage 91.6% lines and 89.1% branches over the engine code.
- Docs: `docs/gates/phase7a.md`, `docs/gates/phase7a-coverage.txt`, `docs/perf/t11/README.md`, `docs/perf/t11-barriers/README.md`; strata `docs/reviews/phase-7a-gate.md`.
- Left open: A8.

## Rulings I made

None beyond the quest file's Decisions (the gate report and the review are the record).
