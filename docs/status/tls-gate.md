# tls-gate
- Design: docs/design/transport-security.md at 668757f02; reviews: docs/reviews/transport-security-design.md (APPROVE WITH CONDITIONS, rev 3)
- Branch / worktree: tls/d1-gate at /Users/kronuz/code/Kronuz/Xapiand-wt/tls-gate
- State: implementing; the code for step 1 and SYS-13 is written, and every row waits for a build at the branch's head (strict host hold)
- Done:
  - D1 implemented (bb4981558); T1 and T2 in `run_all.sh tls` and CI (7e8cccb7d)
  - C5 and C6: refusals counted by reason with reactor's `rejected` as `cap`, metrics, daily expiry check, `test_tls` (069ae1ebc)
  - T3: `unread_reply` fixed, every case holds, in `run_all.sh tls`, documented (93cb512c2)
  - T3 per-row evidence: `OUT=FILE` writes one JSON record per row (case, bound, measured, unit, pass), RSS per connection included; `ONLY` selects every case; the 64-handshake summary no longer reads a 0.0 s close as missing; the fd-limit node runs under `ulimit -n` instead of `preexec_fn` beside threads (76023c31e)
  - T4 preparation: `loadtest.py --ca`, `harness/tls_cost.py`, one short trial run (9f8c62b07)
  - e2e harness writes per-run files, so concurrent runs do not collide (801a055a4)
  - Validity check on `ASN1_TIME_cmp_time_t`, failing closed on its error return (227a1434f)
  - SYS-13: `tls_key_mode_refusal()`, `test_tls` cases, T2 cases, D1.6, the T2 row, the options page, testing.md (3b6b3ee15)
  - T3 per-row JSON (`OUT=`), `ONLY` over every case (76023c31e); Ruff format (df0c511f3)
  - SYS-13's run-as group per the lead's ruling: `demotion()`, `tls_setup(..., runs_as)`, `test_root_start_with_gid`, D1.6, options (66ddc8cce)
  - Step-1 gate report drafted, each row's status stated from saved evidence (`docs/gates/transport-security-step1.md`, 443bf0429)
- Doing: ASan-only build (~/.scratch/build/tls-gate-asan-only) for T1/T2; then T6 and T7
- TSan at afbf3c33d (Apple clang 17, ~/.scratch/build/tls-gate-tsan, `halt_on_error=1`, reports to `.scratch/head/tsan/reports/`, none written): `test_tls` 59 checks; plain e2e GREEN; T1 over HTTPS 335 requests, 570 assertions, 0 failed; T2 24 of 24; T3 12 of 13, the unread reply closed 63.2 s after the client's last write against a 62.5 s allowance (Release: 60.2 s). The case times from the client's last write, so a server still writing replies after it would read late; unverified, recorded for the report
- Next (after `quiet check` clears, at nice 19 and 4 jobs under a light hold):
  1. Release build at the branch's head; `xapiand_test_tls` and ctest; then `PORT=18881 harness/run_all.sh smoke e2e tls` with `OUT=` for T3; fold results into the gate report
  2. Finish the ASan+UBSan build (~/.scratch/build/tls-gate-asan) and run T1 and T2 under it; TSan build and the same
  3. T6 and T7 from the pinned reactor and http sources (~/.scratch/build/tls-gate-lib-{reactor,http})
  4. CI: the branch has no remote, so no CI runs; the lead decides whether it is pushed or opened as a PR (CI triggers on master, pull requests, and dispatch)
  5. Ledger to "in review" once the rows above hold
- Blocked on:
  - T5's UBSan half (the lead's call; C8 made UBSan fatal): under Apple clang 17's `-fsanitize=undefined -fno-sanitize-recover=all` (~/.scratch/build/tls-gate-asan at afbf3c33d), every node aborts on its first error response: `rapidjson/internal/stack.h:117` `Stack::Reserve` computes `stackTop_ + sizeof(T) * count` with `stackTop_` null ("applying non-zero offset 16 to null pointer"), reached from `SearchService::on_error` (`search_views.cc:3171`) → `write_status_response` → `MsgPack::to_string` → `Writer::StartObject`. Pre-existing to step 1: RapidJSON is Tencent's 1.1.0 (f54b0e47), the path has no TLS frame, and the plain node without `--tls-cert` aborts the same way (four reports, all this site, `.scratch/head/asan/reports/`). `test_tls` is clean under it (59 checks). Options: (1) fix `Reserve` in the RapidJSON the build pins (upstream later guards the null stack), recommended, the root cause; (2) build UBSan with `-fsanitize-recover=pointer-overflow` and suppress only `pointer-overflow:rapidjson/internal/stack.h` at runtime, keeping every other check fatal. CI's gcc UBSan may not flag null-plus-offset, so CI's green there would not show the same; not verified
  - Pushing: Xapiand stays local until the owner says
- Findings folded in: C5 (069ae1ebc), C6 (069ae1ebc), C8 T4 CPU source (9f8c62b07), SYS-13 (3b6b3ee15)
- SYS-13 as built: `tls_key_mode_refusal(mode, group, runs_as)` refuses any bit outside 0640 (owner execute and set-id bits included, as PKI's D3 rule reads), and group read unless the file's group is `runs_as`. Unit tests: the mode table, the file path (0640 own group starts, 0660 refused, another group refused when the user has one), and a root start with `--gid`. T2: 0640 own group starts beside 0640 another group (skipped without a second group), 0660, and 0644

## Lead's ruling (2026-10-17, SYS-13, under the owner's standing permission)

SYS-13 means the group the node will run as: a root start must not accept a key owned by root's group. Check against the `--gid` group when one is given, resolved as `demote()` resolves it, otherwise `getegid()`; keep `tls_key_mode_refusal()` pure by passing the target gid; test a root start with `--gid`; commit locally, do not push.

Applied: `demotion()`, split out of `demote()`, resolves the identity once; `main()` passes its gid (or `getegid()` when not root) to `tls_setup()`, then `demote()`s to the same identity. Two cases follow `demote()` rather than the ruling's literal formula, for the lead to confirm or overrule: a root start without `--gid` checks the `--uid` user's group (what `demote()` switches to; `getegid()` there is root's, which the ruling excludes), and a non-root start with `--gid` checks `getegid()` (`demote()` ignores `--gid` unless root). Gate report finding 5.
- Last verified: 2026-10-17 at afbf3c33d (code as 66ddc8cce), Release build ~/.scratch/build/tls-gate-rel: no warnings from `src/tls.*`, `src/main.cc`, `tests/test_tls.cc`; `xapiand_test_tls` 59 checks; ctest 8/8; `PORT=18881 OUT=.scratch/head/t3.json harness/run_all.sh smoke e2e tls` GREEN (T2 24 cases, SYS-13's included; T3 13 cases); T1 alone 335 requests and 570 assertions plain and HTTPS, 0 failed (`.scratch/head/`). Before that, at a build predating 227a1434f, Release build ~/.scratch/build/tls-gate-rel: `PORT=18881 harness/run_all.sh smoke e2e tls` GREEN (T1 335 requests, 570 assertions plain and HTTPS; T2 22 cases; T3 13 cases); ctest 8/8
- Library or build findings (not fixed here):
  - Homebrew LLVM 23 does not build Xapiand on macOS: SDK `usr/include` as plain `-I` via `UUID_INCLUDE_DIR`/`ZLIB_INCLUDE_DIR` (CMakeLists), RapidJSON `document.h:319` const-member assignment (rapidjson library), `XXH_STATIC_LINKING_ONLY` redefined at `src/config.h:289` under -Werror (Xapiand). The sanitizer build uses Apple clang, whose ASan works on this x86_64 host.
  - Reactor's T6 test covers `strict` with an RSA certificate only and checks tickets as settings; http's T7 tests miss the `version` and `cipher` reasons and the unset-callback case (gate report findings 3 and 4)
  - `X509_cmp_current_time` (deprecated in OpenSSL 3) replaced by `ASN1_TIME_cmp_time_t` in `src/tls.cc`; its error return (-2) now fails startup instead of reading as expired. Not compiled yet: both local builds predate it (Release object 11:21, the edit 11:54; the ASan build stopped at 384/520 before `tls.cc`).

## Owner decision (2026-10-17, system review SYS-13)

Accept a TLS key file at mode 0640 only when its group is the service's own group (as Detent does), so Kubernetes secrets mounted with `fsGroup` work; refuse any wider mode. Change `src/tls.cc` and T2's refusal case, and update `docs/design/transport-security.md` D1.6, before the gate report.
