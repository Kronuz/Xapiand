# tls-gate
- Design: docs/design/transport-security.md at 668757f02; reviews: docs/reviews/transport-security-design.md (APPROVE WITH CONDITIONS, rev 3)
- Branch / worktree: tls/d1-gate at /Users/kronuz/code/Kronuz/Xapiand-wt/tls-gate
- State: implementing
- Done:
  - D1 implemented (bb4981558); T1 and T2 in `run_all.sh tls` and CI (7e8cccb7d)
  - C5 and C6: refusals counted by reason with reactor's `rejected` as `cap`, metrics, daily expiry check, `test_tls` (069ae1ebc)
  - T3: `unread_reply` fixed, every case holds, in `run_all.sh tls`, documented (93cb512c2)
  - T4 preparation: `loadtest.py --ca`, `harness/tls_cost.py`, one short trial run (9f8c62b07)
  - e2e harness writes per-run files, so concurrent runs do not collide (801a055a4)
- Doing: ASan+UBSan build at ~/.scratch/build/tls-gate-asan (Homebrew LLVM 23); then T1 TLS and T2 under it
- Next:
  1. T6/T7 rerun from the pinned reactor and http sources (~/.scratch/build/tls-gate-lib-{reactor,http})
  2. Gate report `docs/gates/transport-security-step1.md`
  3. Ledger to "ready to land"
- Blocked on: nothing
- Findings folded in: C5 (069ae1ebc), C6 (069ae1ebc), C8 T4 CPU source (9f8c62b07)
- Last verified: 2026-10-17, Release build ~/.scratch/build/tls-gate-rel: `PORT=18881 harness/run_all.sh smoke e2e tls` GREEN (T1 335 requests, 570 assertions plain and HTTPS; T2 22 cases; T3 13 cases); ctest 8/8
- Library or build findings (not fixed here):
  - With Homebrew LLVM on macOS, `UUID_INCLUDE_DIR` and `ZLIB_INCLUDE_DIR` resolve to the SDK's `usr/include` and go in as `-I`, ahead of libc++'s C wrappers, so Xapian TUs fail (`<cwchar> tried including <wchar.h>`). Local workaround: point both at directories holding only `uuid/` and `zlib.h`/`zconf.h` (worktree `.scratch/sdk-uuid`, `.scratch/sdk-zlib`). A fix would add them as `SYSTEM` includes or skip SDK paths.
  - `src/tls.cc:50-51` uses `X509_cmp_current_time`, deprecated in this OpenSSL 3 (a warning only).
