# Transport security, step 1: HTTPS for the REST API

Draft for the lead. The gate evidence for step 1 (`docs/design/transport-security.md`, D1,
rows T1 to T7) and for the conditions C1 to C8 of the second re-check
(`docs/reviews/transport-security-design.md`), on branch `tls/d1-gate`. D1 itself is
bb4981558; the owner's key-file ruling (SYS-13) is 3b6b3ee15, and the lead's ruling on its
group 66ddc8cce. Every result below is from builds of afbf3c33d, whose source matches
66ddc8cce outside `docs/`; nodes print `git:aae2a9ac8` in their banner, the revision their
build directories were configured at, not the one they compiled. Raw files are under the
worktree's `.scratch/head/` (untracked).

Host: macOS (Darwin 24.6, x86_64), Homebrew OpenSSL 3 (`/usr/local/opt/openssl@3`), Apple
clang 17, newman 6.2.3 (`~/.scratch/newman6`; the `newman` on PATH is 4.5 and the tls layer
skips under it), 2026-10-17, 19:45 to 20:50, under light holds (`lead-builds`, then
`research-bound`), every build and run at `nice -n 19` with at most 4 jobs. Builds, all
`-DTLS=ON -DBUILD_TESTS=ON -DLTO=OFF`:

| Build | Directory | Flags |
|---|---|---|
| Release | `~/.scratch/build/tls-gate-rel` | `-DCMAKE_BUILD_TYPE=Release -DASSERTS=ON` |
| ASan+UBSan | `~/.scratch/build/tls-gate-asan` | `RelWithDebInfo -DASAN=ON -DUBSAN=ON` (`-fno-sanitize-recover=all`) |
| ASan | `~/.scratch/build/tls-gate-asan-only` | `RelWithDebInfo -DASAN=ON` |
| TSan | `~/.scratch/build/tls-gate-tsan` | `RelWithDebInfo -DTSAN=ON` |
| reactor, http | `~/.scratch/build/tls-gate-lib-{reactor,http}` | the pinned sources (reactor e19afd9, http dc3be77) as top-level Release projects |

Sanitizer runs set `halt_on_error=1` and `log_path` to a `reports/` directory per build;
"no report" below means that directory stayed empty. LeakSanitizer does not run on macOS,
so CI's `detect_leaks=1` is not reproduced. Linux: nothing; CI runs on pushes to `master`,
on pull requests, and by dispatch (`.github/workflows/ci.yml:6-10`), and this branch has no
remote, so no CI run exists for it.

## Rows

| Row | Target | Result | Status |
|---|---|---|---|
| T1 The API over TLS | The docs E2E suite over `https://`, every assertion, the same count as plain | Release, one build: plain 335 requests, 570 assertions, 0 failed; HTTPS 335 requests, all `https`, 570 assertions, 0 failed (`t1/plain.json`, `t1/tls.json`, counted from `run.executions`); `run_all.sh smoke e2e tls` GREEN (`run_all_rel.log`) | Pass |
| T2 Refusals | Each refusal beside an accept case that differs in one variable, with the server's logged reason | Release: 24 of 24 (`run_all_rel.log`): an untrusted CA (certificate), plain HTTP (protocol), a raw TLS 1.1 hello (alert 70, version), static RSA on an RSA certificate (cipher), a forced TLS 1.2 renegotiation, the metric cases, startup refused with an expired, not-yet-valid, or mismatched pair, a key at 0640 with another group, at 0660, or at 0644, or `--tls-cert` alone, and started with a key at 0640 and the node's own group (SYS-13; this user has a second group, so no case skipped) | Pass |
| T3 Bounds | The D1.3 bounds at their values; RSS per TLS connection stated | Release: 13 of 13 (`t3.json`, one record per row): the 65th stalled handshake refused at 0.0 s and the 64 dropped by 5.1 s, counted as `handshakes` 1 and `deadline` 64; connection 201 at `--tls-max-connections 200` refused and counted as `cap`; a cap of 1,000 under 512 open files refused at startup; idle after the handshake and after a reply closed at 75.0 s; a stopped body at 60.0 s; an unread reply 60.2 s after the client's last write; trickled headers at 31.6 s; a RESTORE streamed for 140 s completes (200). RSS 20,656 KB idle, 36,232 KB with 200 connections: 77.9 KB per connection. One run; the earlier run (before 227a1434f) gave 66.5 KB, so the figure moves by about a sixth between runs | Pass |
| T4 Cost | Server CPU seconds per 1,000 requests, TLS against plain, within 5%, five interleaved trials; handshakes per second at one core | `tls_cost.py` (9f8c62b07) runs the three workloads; one 2-second trial on a busy host checked the script only | Pending: the lead's measurement |
| T5 Sanitizers | T1 and T2 clean under ASan, UBSan, and TSan | TSan: `test_tls` 59 checks, plain e2e GREEN, T1 over HTTPS 335 requests and 570 assertions with 0 failed, T2 24 of 24; no report. ASan: the same results; no report. ASan+UBSan: `test_tls` 59 checks with no report, but every node aborts on its first error response at a pre-existing RapidJSON defect (finding 6), the plain node included, so T1 and T2 do not complete under UBSan | Partial: ASan and TSan pass; UBSan blocked on finding 6, the lead's call |
| T6 reactor's `strict` | Static RSA and CBC refused, ECDHE-AEAD accepted, no tickets in TLS 1.2 or 1.3, with an RSA and an ECDSA certificate; unchanged without it | `reactor_tls_test` at e19afd9: `tls: ok`, exit 0 (`libs/reactor_tls_test.log`). It covers the RSA certificate only and reads tickets from the context's settings (finding 3) | Pass for what the test covers; the ECDSA case and an observed ticket-less handshake are owed by reactor |
| T7 The http additions | `set_idle_timeout`, `set_header_timeout`, and `on_handshake_failure` fire at their bound and reason, and change nothing when unset | `timeouts_test` at dc3be77: the idle (from the start and after a reply), header, and inactivity timeouts fire, a moving body completes, nothing fires unset; exit 0. `tls_asio_test`: `protocol`, `certificate`, and `deadline` reported; exit 0 (`libs/`). `version` and `cipher` are covered end to end by T2, not here; no unset-callback case (finding 4) | Pass for what the tests cover; the missing reasons and the unset callback are owed by http |

Under the sanitizers T3 is informational (T5 asks for T1 and T2): TSan 12 of 13 and ASan 12
of 13, each missing only the unread reply, at 63.2 s and 65.0 s after the client's last
write against an allowance of 62.5 s (finding 7). Every other bound held under both.

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
# T5: the e2e and tls layers on -DASAN=ON, -DASAN=ON -DUBSAN=ON, and -DTSAN=ON builds, reports to files
ASAN_OPTIONS=halt_on_error=1:abort_on_error=1:log_path=$R/asan UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:log_path=$R/ubsan \
  TSAN_OPTIONS=halt_on_error=1:second_deadlock_stack=1:log_path=$R/tsan \
  PORT=18881 BIN=$SAN_BUILD/bin/xapiand harness/run_all.sh e2e tls
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
| C4 | Idle bound after the handshake; body and reader residuals bounded by inactivity; the cap checked against the file limit | Pass on Release (T3: idle 75.0 s, stopped body 60.0 s, unread reply 60.2 s, the fd-limit refusal); the unread reply runs late under the sanitizers (finding 7) | `src/manager.cc:1010-1012`; `src/tls.cc` (`tls_setup`, the `getrlimit` check); `harness/tls_bounds.py` |
| C5 | A `protocol` reason; connection-cap refusals reported | Pass: T2's metric cases, T3's `handshakes`, `deadline`, and `cap` counts, and `test_tls` | `src/tls.cc` (`tls_refused`, `TlsWatch::tick`); `src/manager.cc:995-1006, 1124-1130`; `tests/test_tls.cc`; `harness/tls_check.sh` metric cases; `harness/tls_bounds.py` |
| C6 | The context built before `demote()`; expiry checked daily, with a metric | Pass: `tls_setup()` after `detach()` and before `demote()`; the daily schedule in `test_tls`; the 3-day certificate warns and its seconds to expiry show in T2 | `src/main.cc:1024, 1122, 1130`; `src/tls.cc` (`tls_check_expiry`, `TlsWatch::tick`); `src/manager.cc:1124-1130, 1793`; `tests/test_tls.cc` |
| C7 | TLS 1.2 session cache decided and tested; `strict` recorded with Detent | Pass: `SSL_SESS_CACHE_OFF` under `strict`, checked by T6's test; Detent's ledger lists the option | reactor `reactor_tls.h:70-75`, `test/tls_test.cc:162`; Detent `docs/DEPENDENCY_UPGRADES.md:57` at 68ab6c6 (line 77 at its head) |
| C8 | UBSan fails on undefined behavior; renegotiation over TLS 1.2; T1's CA and SAN; T4's CPU source | Pass: UBSan aborts on the first report (finding 6 is that working); `s_client -tls1_2` forces 1.2 and renegotiation is refused; the probe uses `--cacert` and newman `--ssl-extra-ca-certs`; the SAN is `localhost` and `127.0.0.1`; CPU from `/proc/<pid>/stat` and `ps -o cputime` | `.github/workflows/ci.yml:100, 106`; `harness/tls_check.sh:158`; `harness/e2e_capture.sh:48-54`; `harness/tls_certs.sh:18`; `harness/tls_cost.py:44-59` |
| SYS-13 | A key at 0640 accepted only with the group the node runs as; wider modes refused | Pass: `test_tls` (the mode table, the file path, a root start with `--gid`) on Release, ASan, ASan+UBSan, and TSan; T2's key-mode cases on Release, ASan, and TSan. No test starts a node as root (finding 5) | `src/tls.cc` (`tls_key_mode_refusal`, `tls_setup`); `src/main.cc` (`demotion`, `demote`, the `tls_setup` call); `tests/test_tls.cc` (`test_key_mode_rule`, `test_setup_key_modes`, `test_root_start_with_gid`); `harness/tls_check.sh` case 6 |
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

6. UBSan (Apple clang 17, `-fno-sanitize-recover=all`) aborts every node on its first
   error response: `rapidjson/internal/stack.h:117`, `Stack::Reserve` computing
   `stackTop_ + sizeof(T) * count` while `stackTop_` is null ("applying non-zero offset 16
   to null pointer"), from `SearchService::on_error` (`search_views.cc:3171`) through
   `write_status_response` and `MsgPack::to_string` into `Writer::StartObject`. It predates
   step 1: RapidJSON is Tencent's 1.1.0 (f54b0e47), no TLS frame is on the stack, and the
   plain node started without `--tls-cert` aborts the same way (four reports, one site,
   `asan/reports/`). The fix belongs in the RapidJSON the build pins; the other way to
   finish T5's UBSan half is to recover and suppress that one check at that one file,
   keeping the rest fatal. CI's gcc UBSan may not flag a null-plus-offset, so a green CI
   job there would not show the same; not verified.
7. T3's unread-reply case times the close from the client's last write. Release closes 60.2
   s after it, TSan 63.2 s, ASan 65.0 s. The lateness grows with the build's slowdown, which
   fits a server still serving requests it had already buffered after the client's last
   write, so its last reply byte, the bound's real start, comes later; unverified, since no
   server-side timestamp is logged. If it holds, the case needs a server-side reference,
   not a larger allowance.

Not run: T4 (the lead's); UBSan's T1 and T2 (finding 6); LeakSanitizer; anything on Linux,
and so CI's sanitizer jobs; C1 on Linux; C3's package builds; a node started as root; the
cluster layers over HTTPS (step 1 leaves node traffic in clear).
