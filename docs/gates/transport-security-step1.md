# Transport security, step 1: HTTPS for the REST API

Draft for the lead. The gate evidence for step 1 (`docs/design/transport-security.md`, D1,
rows T1 to T7) and for the conditions C1 to C8 of the second re-check
(`docs/reviews/transport-security-design.md`), on branch `tls/d1-gate`. D1 itself is
bb4981558; the owner's key-file ruling (SYS-13) is 3b6b3ee15, and the lead's ruling on its group 66ddc8cce. Each row names the build
behind its result. A row passes only with a run at the gate's commit, so every result
below that predates 3b6b3ee15 is owed a rerun, and the rows that need a running server
stay "Not run" until the host frees.

Host: macOS (Darwin 24.6, x86_64), Homebrew OpenSSL 3 (`/usr/local/opt/openssl@3`), Apple
clang, newman 6, 2026-10-17. The one build with results is a Release build
(`-DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON -DASSERTS=ON -DLTO=OFF -DTLS=ON`,
`~/.scratch/build/tls-gate-rel`), linked at 11:23. Its `tls.cc` object is from 11:21, before
069ae1ebc was committed at 11:29, so it carries C5 and C6 but neither 227a1434f (the
validity check) nor 3b6b3ee15 (SYS-13). No build since: a strict host hold
(`strata-trials`, until 07:04 on 2026-10-18) has been active through this draft.

Linux: nothing. CI runs on pushes to `master`, on pull requests, and by dispatch
(`.github/workflows/ci.yml:6-10`); this branch has no remote, so no CI run exists for it.

## Rows

| Row | Target | Result | Build | Status |
|---|---|---|---|---|
| T1 The API over TLS | The docs E2E suite over `https://`, every assertion, the same count as plain | Plain: 335 requests, 570 assertions, 0 failed. HTTPS: 335 requests, all `https`, 570 assertions, 0 failed (`.scratch/t1/plain.json`, `.scratch/t1/tls.json`, 11:49 and 11:51, counted from `run.executions`) | Release, before 227a1434f | Rerun owed at the gate's commit |
| T2 Refusals | Each refusal beside an accept case that differs in one variable, with the server's logged reason | 22 cases passed before SYS-13 (`.scratch/run_all_rel.log`, 11:34): an untrusted CA (certificate), plain HTTP (protocol), a raw TLS 1.1 hello (alert 70, version), static RSA on an RSA certificate (cipher), a forced TLS 1.2 renegotiation, and startup with an expired, not-yet-valid, or mismatched pair, a key at 0640, or `--tls-cert` alone. 3b6b3ee15 replaces the 0640 case: 0640 with the node's group now starts, beside 0640 with another group (skipped without a second group), 0660, and 0644, which are refused. None of these has run | Release, before 227a1434f | Not run for SYS-13's cases; rerun owed for the rest |
| T3 Bounds | The D1.3 bounds at their values; RSS per TLS connection stated | 13 cases passed (`.scratch/run_all_rel.log`): the 65th stalled handshake refused at 0.0 s, the 64 dropped at 5.0 s (first and last), counted as `handshakes` 1 and `deadline` 64; connection 201 at `--tls-max-connections 200` refused and counted as `cap`; a cap of 1,000 under 512 open files refused at startup; idle after the handshake and after a reply closed at 75.0 s; a stopped body at 60.0 s; an unread reply 60.6 s after the client's last write; trickled headers at 31.5 s; a RESTORE streamed for 140 s completes (200). RSS: 25,644 KB idle, 38,944 KB with 200 connections, 66.5 KB per connection. One run; the per-row JSON (`OUT=`, 76023c31e) has not been produced | Release, before 227a1434f | Rerun owed, with `OUT=` |
| T4 Cost | Server CPU seconds per 1,000 requests, TLS against plain, within 5%, five interleaved trials; handshakes per second at one core | `tls_cost.py` (9f8c62b07) runs the three workloads. One 2-second trial on a busy host checked the script only (`.scratch/tls_cost_check.json`); it is not evidence | | Pending: the lead's measurement on a quiet host |
| T5 Sanitizers | T1 and T2 clean under ASan, UBSan, and TSan | Not run. The local ASan+UBSan build (`~/.scratch/build/tls-gate-asan`, Apple clang) stopped at 384 of 520 steps, before `tls.cc`. CI's ASan+UBSan and TSan jobs run the `tls` layer with `halt_on_error=1` (`ci.yml:100, 104-106, 131-132`), but no CI run exists for this branch | | Not run |
| T6 reactor's `strict` | Static RSA and CBC refused, ECDHE-AEAD accepted, no tickets in TLS 1.2 or 1.3, with an RSA and an ECDSA certificate; unchanged without it | Not run: `~/.scratch/build/tls-gate-lib-reactor` is configured only. The test exists at reactor e19afd9 (`test/tls_test.cc:146-166`); see finding 3 for what it covers | | Not run |
| T7 The http additions | `set_idle_timeout`, `set_header_timeout`, and `on_handshake_failure` fire at their bound and reason, and change nothing when unset | Not run: `~/.scratch/build/tls-gate-lib-http` is configured only. The tests exist at http dc3be77 (`test/timeouts_test.cc:76-138`, `test/tls_asio_test.cc:150-164`); see finding 4 | | Not run |

## Rerunning each row

```sh
B=build/bin/xapiand   # a -DTLS=ON -DBUILD_TESTS=ON build; newman 6 first on PATH
# T1, T2, T3: the tls layer (e2e over HTTPS on :18880, tls_check.sh on :18890, tls_bounds.py on :18895)
PORT=18881 BIN=$B harness/run_all.sh smoke e2e tls
# T1 alone, plain and HTTPS, with counts
PORT=18882 COLL=c1.json harness/e2e_capture.sh $B plain.json && python3 harness/e2e_assert.py plain.json
harness/tls_certs.sh certs && PORT=18883 COLL=c2.json SCHEME=https CA=certs/ca.pem \
  XAPIAND_FLAGS="--tls-cert certs/server.pem --tls-key certs/server.key" \
  harness/e2e_capture.sh $B tls.json && python3 harness/e2e_assert.py tls.json
harness/tls_check.sh $B                                # T2, with the C5/C6 metric cases and SYS-13's
OUT=t3.json python3 -I harness/tls_bounds.py $B        # T3, about 2.5 minutes, one record per row
# T4, on a quiet host only
python3 -I harness/tls_cost.py $B --trials 5 --duration 30 --out cost.json
# T5: the tls layer on an -DASAN=ON -DUBSAN=ON build, then on -DTSAN=ON
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ASAN_OPTIONS=halt_on_error=1:abort_on_error=1 \
  PORT=18881 BIN=$ASAN_BUILD/bin/xapiand harness/run_all.sh e2e tls
# T6, T7: the pinned library sources as top-level projects
cmake -GNinja -S $BUILD/_deps/reactor-src -B lib-reactor && ninja -C lib-reactor reactor_tls_test && lib-reactor/reactor_tls_test
cmake -GNinja -S $BUILD/_deps/http_lib-src -B lib-http && ninja -C lib-http timeouts_test tls_asio_test \
  && lib-http/timeouts_test && lib-http/tls_asio_test
# C5, C6, SYS-13 unit test
$BUILD/bin/xapiand_test_tls
```

## Conditions

| ID | Condition | Status | Where |
|---|---|---|---|
| C1 | Multi-node, recovery, shutdown, and signal layers on the step-0 commit, Linux and macOS, without the local override; ranges listed with checks | Partial: macOS pass; Linux not run | `docs/gates/transport-security-step0.md` ("What moves", "Results") |
| C2 | Every pin disagreement resolved in the lagging library | Pass: only journal and consensus disagree, and Xapiand includes neither | `docs/gates/transport-security-step0.md` ("Pin consistency") |
| C3 | Docker, Homebrew, and FreeBSD packages built without publishing; each platform's OpenSSL stated | Open: not built | `docs/gates/transport-security-step0.md` (last Results row, empty) |
| C4 | Idle bound after the handshake; body and reader residuals bounded by inactivity; the cap checked against the file limit | Built; ran before 227a1434f (T3's idle 75.0 s, stopped body 60.0 s, unread reply 60.6 s, and the fd-limit refusal). Rerun owed with T3 | `src/manager.cc:1010-1012`; `src/tls.cc` (`tls_setup`, the `getrlimit` check); `harness/tls_bounds.py` |
| C5 | A `protocol` reason; connection-cap refusals reported | Built (069ae1ebc); ran before 227a1434f in T2 and T3. `test_tls` not run since | `src/tls.cc` (`tls_refused`, `TlsWatch::tick`); `src/manager.cc:995-1006, 1124-1130`; `tests/test_tls.cc`; `harness/tls_check.sh` metric cases; `harness/tls_bounds.py` cap and handshake metrics |
| C6 | The context built before `demote()`; expiry checked daily, with a metric | Built (069ae1ebc): `tls_setup()` after `detach()` and before `demote()`; a control-plane timer re-checks expiry every 24 hours; `xapiand_tls_certificate_expiry_seconds`. Ran before 227a1434f in T2 case 7 | `src/main.cc:1002, 1097, 1105`; `src/tls.cc` (`tls_check_expiry`, `TlsWatch::tick`); `src/manager.cc:1124-1130, 1793`; `tests/test_tls.cc` (daily schedule) |
| C7 | TLS 1.2 session cache decided and tested; `strict` recorded with Detent | Decided and recorded: `SSL_SESS_CACHE_OFF` under `strict`; Detent's ledger lists the option. Its test is T6's, not run | reactor `reactor_tls.h:70-75`, `test/tls_test.cc:162`; Detent `docs/DEPENDENCY_UPGRADES.md:57` at 68ab6c6 (line 77 at its head) |
| C8 | UBSan fails on undefined behavior; renegotiation over TLS 1.2; T1's CA and SAN; T4's CPU source | In place: `-DUBSAN=ON` adds `-fno-sanitize-recover=all` and CI sets `halt_on_error=1`; `s_client -tls1_2` forces 1.2; the probe uses `--cacert` and newman `--ssl-extra-ca-certs`; the certificate's SAN is `localhost` and `127.0.0.1`; CPU from `/proc/<pid>/stat` and `ps -o cputime`. The UBSan path has not run (T5) | `.github/workflows/ci.yml:100, 106`; `harness/tls_check.sh:158`; `harness/e2e_capture.sh:48-54`; `harness/tls_certs.sh:18`; `harness/tls_cost.py:44-59` |
| SYS-13 | A key at 0640 accepted only with the group the node runs as; wider modes refused | Built (3b6b3ee15; the run-as group per the lead's ruling, 66ddc8cce), not compiled or run: `tls_key_mode_refusal()` with its unit tests, `demotion()` shared with `demote()`, T2's cases, D1.6, the T2 row, and the options page | `src/tls.cc` (`tls_key_mode_refusal`, `tls_setup`); `src/main.cc` (`demotion`, `demote`, the `tls_setup` call); `tests/test_tls.cc` (`test_key_mode_rule`, `test_setup_key_modes`, `test_root_start_with_gid`); `harness/tls_check.sh` case 6 |
| TS-R24 | The user docs and the changelog say node traffic stays in clear | Pass | `docs/src/content/docs/options.md:89-92`; `docs/src/content/docs/changelog.md:14-17` |

## Findings

1. Homebrew LLVM 23 on macOS does not build Xapiand, for three reasons, none from this step:
   `UUID_INCLUDE_DIR` and `ZLIB_INCLUDE_DIR` resolve to the SDK's `usr/include` and go in as
   plain `-I`, ahead of libc++'s C wrappers (`<cwchar> tried including <wchar.h>`); RapidJSON's
   `GenericStringRef::operator=` assigns a const member, which clang 23 rejects even
   uninstantiated (`rapidjson/document.h:319`); and `src/config.h:289` redefines
   `XXH_STATIC_LINKING_ONLY` under `-Werror`. Apple clang and CI's gcc are unaffected. The
   first is `CMakeLists.txt`'s, the second the rapidjson library's, the third Xapiand's.
2. `e2e_capture.sh` kills, with `-9`, whatever holds its port before it starts; on a shared
   host that is another agent's node unless `PORT` is set. The `tls` layer sets its ports;
   `run_all.sh e2e` uses `PORT` from the environment. Not changed here.
3. T6's test at reactor e19afd9 checks `strict` with an RSA certificate only, while the row
   asks for an RSA and an ECDSA one; and it checks tickets as context settings
   (`SSL_OP_NO_TICKET`, `num_tickets == 0`), not by a handshake that issues none. Both are
   reactor's to add.
4. T7's tests at http dc3be77 fire `on_handshake_failure` for `protocol`, `certificate`, and
   `deadline`, but not for `version` or `cipher`, and do not check that nothing fires
   without a callback. T2 covers `version` and `cipher` through Xapiand's callback end to
   end. The rest is http's to add.
5. SYS-13's group, ruled by the lead under the owner's standing permission (2026-10-17):
   the group the node will run as, so a root start never accepts a key owned by root's
   group. Built as `demotion()`, split out of `demote()`, which resolves the identity once;
   `main()` passes its gid to `tls_setup()` and then demotes to the same identity, so the
   check and the switch cannot disagree. Where the ruling's literal wording ("`--gid` when
   given, otherwise `getegid()`") and `demote()`'s resolution differ, the code follows
   `demote()`, which is what the node runs as: a root start without `--gid` checks the
   `--uid` user's group (`getegid()` there is root's), and a non-root start checks
   `getegid()` even when `--gid` is given (`demote()` ignores `--gid` unless root). The lead
   may overrule either. `test_root_start_with_gid` covers the root start at the rule and
   through `tls_setup()`; no test starts a node as root, so `demotion()`'s lookups are
   exercised only as `demote()`'s were.

Not run: every row at a commit carrying 227a1434f, 3b6b3ee15, and 66ddc8cce; T2's SYS-13 cases;
`test_tls`; T4's measurement (the lead's, on a quiet host); T5 entirely (the local ASan
build is unfinished, TSan not started, and no CI run exists for the branch); T6 and T7;
anything on Linux; C1 on Linux; C3's package builds; the cluster layers over HTTPS (step 1
leaves node traffic in clear).
