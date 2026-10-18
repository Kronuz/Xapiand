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
- Doing: SYS-13 in `src/tls.cc` with its unit test and T2 cases; then T3's per-row output and the step-1 gate report. No compile or test this session: a strict host hold (`strata-trials`, until 07:04 on 2026-10-18)
- Next:
  1. T6/T7 rerun from the pinned reactor and http sources (~/.scratch/build/tls-gate-lib-{reactor,http})
  2. Gate report `docs/gates/transport-security-step1.md`
  3. Ledger to "ready to land"
- Blocked on: nothing
- Findings folded in: C5 (069ae1ebc), C6 (069ae1ebc), C8 T4 CPU source (9f8c62b07), SYS-13 (this step)
- SYS-13 as built: `tls_key_mode_refusal(mode, group, egid)` refuses any bit outside 0640 (owner execute and set-id bits included, as PKI's D3 rule reads), and group read unless the file's group is `getegid()` at the check. `tls_setup()` runs before `demote()`, so a node started as root checks against root's group, not `--gid`'s; the ruling's "effective group" is applied as written, and the lead may want it confirmed for the root-start case. Unit test: the mode table and the file path (0640 own group starts, 0660 refused, another group refused when the user has one). T2: 0640 own group starts beside 0640 another group (skipped without a second group), 0660, and 0644
- Last verified: 2026-10-17, Release build ~/.scratch/build/tls-gate-rel: `PORT=18881 harness/run_all.sh smoke e2e tls` GREEN (T1 335 requests, 570 assertions plain and HTTPS; T2 22 cases; T3 13 cases); ctest 8/8
- Library or build findings (not fixed here):
  - Homebrew LLVM 23 does not build Xapiand on macOS: SDK `usr/include` as plain `-I` via `UUID_INCLUDE_DIR`/`ZLIB_INCLUDE_DIR` (CMakeLists), RapidJSON `document.h:319` const-member assignment (rapidjson library), `XXH_STATIC_LINKING_ONLY` redefined at `src/config.h:289` under -Werror (Xapiand). The sanitizer build uses Apple clang, whose ASan works on this x86_64 host.
  - `X509_cmp_current_time` (deprecated in OpenSSL 3) replaced by `ASN1_TIME_cmp_time_t` in `src/tls.cc`; its error return (-2) now fails startup instead of reading as expired. Not compiled yet: both local builds predate it (Release object 11:21, the edit 11:54; the ASan build stopped at 384/520 before `tls.cc`).

## Owner decision (2026-10-17, system review SYS-13)

Accept a TLS key file at mode 0640 only when its group is the service's own group (as Detent does), so Kubernetes secrets mounted with `fsGroup` work; refuse any wider mode. Change `src/tls.cc` and T2's refusal case, and update `docs/design/transport-security.md` D1.6, before the gate report.
