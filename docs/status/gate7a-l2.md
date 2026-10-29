> Status: CURRENT (2026-10-29) gate 7a, blocker L2 and conditions C1, C2, C4 of `docs/reviews/phase-7a-gate.md` (strata); branch `phase7a/gate-l2` from master `3bc11f31f`; workstream gate7a-l2, developer.

# Gate 7a: L2 (every-gate checks) and C1, C2, C4

## State

Done, ready for the rechecker. Results are in `docs/gates/phase7a.md`: "Every-gate checks", "D4: why `_version` is 2", "A text query that
never finishes", and the corrected T11 text of the closing run. One new finding came out of the fuzz run (a text query that hangs the
server); it is fixed at the lowering here and needs a fix in `Kronuz/queryparser` (the lead's).

- L2: T10 under TSan, clean (611 s and 307 s mixed loads); ctest 28 of 28 under TSan, and under ASan+UBSan once RapidJSON's `stack.h:117` is
  patched locally (stock: 26 of 28, the known pre-existing finding); docs E2E under ASan+UBSan 566 of 566 on both engines; fuzz
  1 CPU-hour for each of the four Xapiand targets (one found the hang); coverage 91.6% lines, 89.1% branches over the engine code, files
  below target listed. Deferrals with reason, owner and phase are in the report's last table of that section.
- C1: `profile_bulk.sh` reports device-flush wait as its own line; on CPU the T11 row is 0.357x (closing) and 0.414x (Rerun).
- C2: `xapian_1` of hold 1 is marked invalid; the harness rejects a profile under 25% of the sampling window or half its siblings' median.
- C4: traced to a term-prefix collision (shard-local docid 78 and the schema's `_version` field terms), reproduced on
  `v1.0.0-alpha.9`, fixed in `stored_version`.

## Done (commits, all in Xapiand)

- Xapiand:db2a9d813 `profile_bulk.sh`: flush wait apart from CPU, truncated profiles rejected (C1, C2)
- Xapiand:bf25fb518 `harness/engine_mixed_load.py`: the T10 mixed load
- Xapiand:949180020 T11 text corrected (C1, C2)
- Xapiand:1839276a4 Xapian `stored_version` reads the value slot; test `version_term_collision` (C4)
- Xapiand:e17bfc501 D4 trace and alpha.9 control in the report (C4)
- Xapiand:18c70a46e local sanitizer recipe in `docs/dev/testing.md`
- Xapiand:0e01adabc `convert` ends on a self-linked tree; test `test_hate_groups_terminate`; fuzz watchdog and replay
- Xapiand:d4fa897d6, Xapiand:1b94f8d2e, Xapiand:03b1a29d8 report and changelog

## Next

1. The lead: have `Kronuz/queryparser` fixed (a node added twice to one parent, `queryparser.cc:189-199`) and re-pinned; add a cycle
   check to its fuzz target.
2. The lead: CI for Xapiand (`phase*/**` triggers; the strata engine under the sanitizer jobs); see the deferral table.
3. The recheck of L2, C1, C2, C4 by reading the report section, `.scratch/gate7a-l2/` (kept in the main checkout) and rerunning
   `harness/profile_bulk.sh --reanalyze` on the closing run's raw samples.

## Blocked on

Nothing.
