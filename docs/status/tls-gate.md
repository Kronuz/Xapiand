# tls-gate
- Design: docs/design/transport-security.md at 668757f02; reviews: docs/reviews/transport-security-design.md (APPROVE WITH CONDITIONS, rev 3)
- Branch / worktree: tls/d1-gate at /Users/kronuz/code/Kronuz/Xapiand-wt/tls-gate
- State: in review. Every step-1 row run at e857e12f4 except T4 (the lead's); gate report `docs/gates/transport-security-step1.md`
- Done:
  - D1 implemented (bb4981558); T1 and T2 in `run_all.sh tls` and CI (7e8cccb7d)
  - C5 and C6: refusals counted by reason with reactor's `rejected` as `cap`, metrics, daily expiry check, `test_tls` (069ae1ebc)
  - T3: `unread_reply` fixed, every case holds, in `run_all.sh tls`, documented (93cb512c2)
  - T4 preparation: `loadtest.py --ca`, `harness/tls_cost.py`, one short trial run (9f8c62b07)
  - e2e harness writes per-run files, so concurrent runs do not collide (801a055a4)
  - Validity check on `ASN1_TIME_cmp_time_t`, failing closed on its error return (227a1434f)
  - SYS-13: `tls_key_mode_refusal()`, `test_tls` cases, T2 cases, D1.6, the T2 row, the options page, testing.md (3b6b3ee15)
  - T3 per-row JSON (`OUT=`), `ONLY` over every case, the 0.0 s summary fix, `ulimit -n` for the fd-limit node (76023c31e); Ruff format (df0c511f3)
  - SYS-13's run-as group per the lead's ruling: `demotion()`, `tls_setup(..., runs_as)`, `test_root_start_with_gid`, D1.6, options (66ddc8cce)
  - Finding 7, per the lead's ruling: `unread_reply` times the close from the server's last byte read or written, seen in its socket's queues (f119ffe1e)
  - Finding 6, per the lead's ruling: rapidjson pinned at 16872af8, the earliest commit without the null-pointer arithmetic, reason beside the pin (e857e12f4)
  - Step-1 gate report: drafted (443bf0429), results at afbf3c33d (1b409212a), results at e857e12f4 with findings 6 to 8 resolved or noted (9d2881b5e)
- Doing: nothing
- Next:
  1. The code review and the security review the tier asks for (the lead launches them)
  2. T4, the lead's measurement on a quiet host
  3. Linux and CI when the owner allows a push or PR (CI triggers on master, pull requests, and dispatch)
  4. Fold in review findings, citing their IDs
- Blocked on: pushing; Xapiand stays local until the owner says
- Findings folded in: C5 (069ae1ebc), C6 (069ae1ebc), C8 T4 CPU source (9f8c62b07), SYS-13 (3b6b3ee15, 66ddc8cce), gate report finding 6 (e857e12f4), finding 7 (f119ffe1e)
- Last verified: 2026-10-17 at e857e12f4 (harness as f119ffe1e), Apple clang 17, at nice 19 and 4 jobs under light holds; raw files under `.scratch/head/`:
  - Release (~/.scratch/build/tls-gate-rel): no rapidjson warning; `xapiand_test_tls` 59 checks; ctest 8/8; T1 335 requests and 570 assertions plain and HTTPS, 0 failed, response bodies matching the v1.1.0 build's but for the three volatile ones (`bump/`); T2 25 of 25; T3 12 of 12, unread reply 60.0 s from the server's last byte, RSS 70.7 KB per connection
  - ASan+UBSan, `-fno-sanitize-recover=all` (~/.scratch/build/tls-gate-asan): `test_tls` 59; plain docs E2E and T1 over HTTPS 335 and 570, 0 failed; T2 25 of 25; T3 12 of 12; no report (`ubsan/`)
  - TSan (~/.scratch/build/tls-gate-tsan): the same; no report (`tsan-bump/`)
  - T6 `reactor_tls_test` (e19afd9) and T7 `timeouts_test`, `tls_asio_test` (http dc3be77): exit 0 (`libs/`)
- Count corrections: T2 has 25 cases since 3b6b3ee15 (22 before); T3 has 12 checks plus the RSS record. Earlier ledger lines and commit messages (5ff9e41f7, 4acb96436, 1b409212a) said 24 and 13
- SYS-13 as built: `tls_key_mode_refusal(mode, group, runs_as)` refuses any bit outside 0640 (owner execute and set-id bits included, as PKI's D3 rule reads), and group read unless the file's group is `runs_as`. Unit tests: the mode table, the file path (0640 own group starts, 0660 refused, another group refused when the user has one), and a root start with `--gid`. T2: 0640 own group starts beside 0640 another group (skipped without a second group), 0660, and 0644
- Library or build findings (not fixed here):
  - Homebrew LLVM 23 does not build Xapiand on macOS: SDK `usr/include` as plain `-I` via `UUID_INCLUDE_DIR`/`ZLIB_INCLUDE_DIR` (CMakeLists), RapidJSON `document.h:319` const-member assignment (rapidjson library), `XXH_STATIC_LINKING_ONLY` redefined at `src/config.h:289` under -Werror (Xapiand). The sanitizer builds use Apple clang, whose ASan works on this x86_64 host
  - Reactor's T6 test covers `strict` with an RSA certificate only and checks tickets as settings; http's T7 tests miss the `version` and `cipher` reasons and the unset-callback case (gate report findings 3 and 4)
  - Xapiand pins rapidjson at 16872af8 and Kronuz/msgpack at 24b5e7a8; Xapiand's first declaration governs, and `pin_consistency.py` compares only `github.com/Kronuz/*` pins, so it does not see the disagreement (gate report finding 6)
  - Node banners print the git revision their build directory was configured at (`get_git_hash()`, `CMakeLists.txt:1572`), not the one compiled (gate report finding 8)

## Owner decision (2026-10-17, system review SYS-13)

Accept a TLS key file at mode 0640 only when its group is the service's own group (as Detent does), so Kubernetes secrets mounted with `fsGroup` work; refuse any wider mode. Change `src/tls.cc` and T2's refusal case, and update `docs/design/transport-security.md` D1.6, before the gate report.

## Lead's ruling (2026-10-17, SYS-13, under the owner's standing permission)

SYS-13 means the group the node will run as: a root start must not accept a key owned by root's group. Check against the `--gid` group when one is given, resolved as `demote()` resolves it, otherwise `getegid()`; keep `tls_key_mode_refusal()` pure by passing the target gid; test a root start with `--gid`; commit locally, do not push.

Applied: `demotion()`, split out of `demote()`, resolves the identity once; `main()` passes its gid (or `getegid()` when not root) to `tls_setup()`, then `demote()`s to the same identity. Two cases follow `demote()` rather than the ruling's literal formula, for the lead to confirm or overrule: a root start without `--gid` checks the `--uid` user's group (what `demote()` switches to; `getegid()` there is root's, which the ruling excludes), and a non-root start with `--gid` checks `getegid()` (`demote()` ignores `--gid` unless root). Gate report finding 5.

## Lead's rulings (2026-10-17, gate report findings 6 to 8)

- Finding 6: fix the root cause; pin the earliest upstream rapidjson commit that has the `Stack::Reserve` fix and builds cleanly, the reason beside the pin; rerun T1 and T2 under ASan+UBSan with `-fno-sanitize-recover=all`, T1 in Release, and the plain docs E2E suite; list Kronuz libraries that declare rapidjson. Applied in e857e12f4: 16872af8; only msgpack declares it, at 24b5e7a8, which has the fix
- Finding 7: fix the harness; measure the unread reply from the server's last reply byte, keep the 60 s bound. Applied in f119ffe1e from the server socket's queues, since the client's last arrival reads early (gate report finding 7)
- Banner: a one-line note in the report, no fix now (finding 8)
- Then the ledger to "in review"; commit locally, no push
