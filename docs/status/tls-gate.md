# tls-gate
- Design: docs/design/transport-security.md at 668757f02; reviews: docs/reviews/transport-security-design.md (APPROVE WITH CONDITIONS, rev 3)
- Branch / worktree: tls/d1-gate at /Users/kronuz/code/Kronuz/Xapiand-wt/tls-gate
- State: implementing
- Done:
  - D1 implemented (bb4981558); T1 and T2 in `run_all.sh tls` and CI (7e8cccb7d); T3 probe WIP (090a6fd53)
- Doing: T3, fixing `unread_reply` in `harness/tls_bounds.py` (writer thread, no timeout, time the server's close)
- Next:
  1. T3 wired into `run_all.sh tls`, documented in `docs/dev/testing.md`
  2. C5: failure reasons plus reactor's `rejected` counted, as metrics, logged once a minute per reason
  3. C6: daily expiry check on the control-plane timer, seconds-to-expiry metric
  4. T4 preparation: `loadtest.py` HTTPS, CPU-seconds trial script (one short trial only)
  5. ASan+UBSan build: T1 TLS and T2
  6. Gate report `docs/gates/transport-security-step1.md`
- Blocked on: nothing
- Findings folded in: none yet
- Last verified: Release build at ~/.scratch/build/tls-gate-rel configured 2026-10-17
