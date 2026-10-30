> Status: CURRENT (2026-10-30) the X2b developer's ledger (phase 7c step X2b: the handler, views and schema callers as tasks, and the reader views); in progress.

# phase7c X2b (build): handlers and views as tasks, reader views

Branch `phase7c/x2b-handlers-views` in `Xapiand-wt/x2b` (local only, from `phase7c/x2-registry-lease` at `Xapiand:7951486a9`, rebased onto the
X2 review `Xapiand:db50b90b3`). Design: `docs/design/phase7c.md` (D2, D3, D5, D6, C9, C16); X2's ledger: `docs/status/phase7c-x2.md`; X2 review:
`docs/reviews/phase7c-x2-code.md`. Mechanism notes: `docs/mechanisms/reader-views.md`, `docs/mechanisms/index-registry-and-lease.md`.

## State

In progress. The engine's reader views, the pool's reader registry, the handler, the schema and resolver code and the views are tasks; the
tests and the proof matrix are being run (see "Done criteria"). Nothing is blocked.

## Done criteria

(Each is ticked with its evidence when it holds.)

- [ ] **K1.** No `sync_wait` on a server path (grep), and `harness/async_check.sh --strict` reports 0.
- [ ] **K2.** Reader views replace `core.mtx` and `Pinned`. A test shows a search held for 1 s does not delay a concurrent refresh or commit, and
  fails with the old locking.
- [ ] **K3.** C16's held-guard test.
- [ ] **K4.** `too_many_readers` returns 503, tested with a small registry.
- [ ] **K5. Proof.** Unit tests on Release, Debug and ASan+UBSan; TSan on the concurrency tests plus `engine_mixed_load.py` for 120 s; docs E2E
  641 of 641; `wal_recovery_check.sh` and `engine_crash_check.sh --reactors 2`; `graceful_shutdown_check.sh`; a mutant per new rule.
- [ ] **K6.** The note and the changelog.

X2 review items folded in: X2-B1, X2-N1, X2-N2, X2-N3, X2-N8 (see the log).

## Done (commits)

## Next

## Blocked on

Nothing.

## Log

### 2026-10-30

- Read the design (D2, D3, D5, D6, C9, C16), X2's ledger, the registry note and the X2 review; strata's `index.hh` reader handle, hazard registry and
  `defer_io` note; task's `task.hh`.
