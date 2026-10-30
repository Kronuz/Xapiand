> Status: CURRENT (2026-10-30) the X3 developer's ledger (phase 7c step X3: views on the reactors as coroutine handlers); in progress, not pushed.

# phase7c X3 (build): views on the reactors

Branch `phase7c/x3-views` in `Xapiand-wt/x3` (local only, from master `Xapiand:d25fb70a4`). Design: `docs/design/phase7c.md` (D2, D3, D9, D12, D16, T rows, section 5's X3);
the architect's ruling and the X3 items: `docs/reviews/phase7c-x2b-code.md` (X3-1 to X3-5); http's L3 ledger `docs/status/phase7c-l3.md`, "What X3 must change".

## State

Step 1 (pins and the coroutine service) done and committed. Steps 2 to 4 follow the order in "Done criteria".

## Done criteria

- [ ] **1. Pins and service.**
- [ ] **2. Views on the reactor** (X3-2, X3-3, X3-4; X3-1 and X3-5 remain).
- [ ] **3. Bodies and blocking paths** (`body_stream`, RESTORE parse-ahead, streamed uploads, FLUSH, bulk indexers).
- [ ] **4. Proof.**

## Done (commits)

## Next

## Blocked on

Nothing.

## Log

### 2026-10-30

- Pins moved to the heads the task names (reactor 76c32b4, http fce24c6, http-log d2315b8, task 9a85e89, threadpool e961e44, strata c82ac9f, prospect c77d6eb, storage e4c5746,
  compressors dc4474b).
