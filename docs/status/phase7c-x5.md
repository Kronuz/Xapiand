> Status: CURRENT (2026-11-02) the X5 developer's ledger (phase 7c step X5: the schema and settings caches, the control plane as tasks, inventory J); built, proof in progress; not pushed.

# phase7c X5 (build): the caches, the control plane and inventory J

Branch `phase7c/x5-control` in `Xapiand-wt/x5` (local only, from master `Xapiand:d5a2e746c`). Design: `docs/design/phase7c.md` (D1, D11, D13, D14, D16, section 5's X5); the review: `docs/reviews/phase7c-design.md`
(C10, C14); what X2, X2b and X3 converted: `docs/status/phase7c-x2.md`, `phase7c-x2b.md`, `phase7c-x3.md`. The mechanism note is `docs/mechanisms/control-plane.md`.

## State

Building and proving. Nothing is blocked.

## Done criteria

- [ ] **K1.** The caches hold no lock across I/O; concurrent first use of one schema loads it once.
- [ ] **K2.** The control plane's work runs as tasks on the control loop; a lag probe shows the loop free during a slow commit and a schema load.
- [ ] **K3.** Inventory J classified; `async_check.sh --strict` 0 unclassified, no owed hits left for D1, D11 and D13.
- [ ] **K4.** C10 and C14.
- [ ] **K5.** Proof.

## Done (commits)

## Next

## Blocked on

Nothing.

## Log
