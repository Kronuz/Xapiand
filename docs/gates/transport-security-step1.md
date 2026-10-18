# Transport security, step 1: HTTPS for the REST API

Draft for the lead. The gate evidence for step 1 (`docs/design/transport-security.md`, D1,
rows T1 to T7) and for the conditions C1 to C8 of the second re-check
(`docs/reviews/transport-security-design.md`), on branch `tls/d1-gate`. D1 itself is
bb4981558; the owner's key-file ruling (SYS-13) is 3b6b3ee15, and the lead's ruling on its
group 66ddc8cce. Every result below is from builds of e857e12f4, which pins rapidjson at
16872af8 (finding 6), with the T3 harness of f119ffe1e (finding 7). Raw files are under the
worktree's `.scratch/head/` (untracked): `bump/` (Release), `ubsan/` (ASan+UBSan),
`tsan-bump/` (TSan), and `libs/` (T6, T7).

Host: macOS (Darwin 24.6, x86_64), Homebrew OpenSSL 3 (`/usr/local/opt/openssl@3`), Apple
clang 17, newman 6.2.3 (`~/.scratch/newman6`; the `newman` on PATH is 4.5 and the tls layer
skips under it), 2026-10-17, 19:45 to 21:45, under light holds, every build and run at
`nice -n 19` with at most 4 jobs. Builds, all `-DTLS=ON -DBUILD_TESTS=ON -DLTO=OFF`:

| Build | Directory | Flags |
|---|---|---|
| Release | `~/.scratch/build/tls-gate-rel` | `-DCMAKE_BUILD_TYPE=Release -DASSERTS=ON` |
| ASan+UBSan | `~/.scratch/build/tls-gate-asan` | `RelWithDebInfo -DASAN=ON -DUBSAN=ON` (`-fno-sanitize-recover=all`) |
| TSan | `~/.scratch/build/tls-gate-tsan` | `RelWithDebInfo -DTSAN=ON` |
| reactor, http | `~/.scratch/build/tls-gate-lib-{reactor,http}` | the pinned sources (reactor e19afd9, http dc3be77) as top-level Release projects |

Sanitizer runs set `halt_on_error=1` and `log_path` to a `reports/` directory per build;
"no report" means that directory stayed empty. LeakSanitizer does not run on macOS, so CI's
`detect_leaks=1` is not reproduced. Linux: nothing; CI runs on pushes to `master`, on pull
requests, and by dispatch (`.github/workflows/ci.yml:6-10`), and this branch has no remote,
so no CI run exists for it.

## Rows

| Row | Target | Result | Status |
|---|---|---|---|
| T1 The API over TLS | The docs E2E suite over `https://`, every assertion, the same count as plain | Release, one build: plain 335 requests, 570 assertions, 0 failed; HTTPS 335 requests, all `https`, 570 assertions, 0 failed (`bump/plain.json`, `bump/tls.json`, counted from `run.executions`). Against the v1.1.0 build's captures, response bodies differ only in the three `e2e_check.sh` names as volatile: an auto-id, `GET /`'s `server.revision` and `versions.Xapiand` (the build stamp; its keys are identical), and `/:metrics` (`bump/bodydiff-{plain,tls}.txt`), so the bump changes no JSON response | Pass |
| T2 Refusals | Each refusal beside an accept case that differs in one variable, with the server's logged reason | 25 of 25 on Release, ASan+UBSan, and TSan (`bump/t2.log`, `ubsan/run.log`, `tsan-bump/run.log`): an untrusted CA (certificate), plain HTTP (protocol), a raw TLS 1.1 hello (alert 70, version), static RSA on an RSA certificate (cipher), a forced TLS 1.2 renegotiation, the metric cases, startup refused with an expired, not-yet-valid, or mismatched pair, a key at 0640 with another group, at 0660, or at 0644, or `--tls-cert` alone, and started with a key at 0640 and the node's own group (SYS-13; this user has a second group, so no case skipped) | Pass |
| T3 Bounds | The D1.3 bounds at their values; RSS per TLS connection stated | Release, 12 of 12 checks, and RSS stated (`bump/t3.json`, one record per row): the 65th stalled handshake refused at 0.0 s and the 64 dropped at 5.0 s, counted as `handshakes` 1 and `deadline` 64; connection 201 at `--tls-max-connections 200` refused and counted as `cap`; a cap of 1,000 under 512 open files refused at startup; idle after the handshake and after a reply closed at 75.0 s; a stopped body at 60.0 s; an unread reply at 60.0 s after the server's last read or write (60.2 s after the client's last write); trickled headers at 31.6 s; a RESTORE streamed for 140 s completes (200). RSS 24,540 KB idle, 38,684 KB with 200 connections: 70.7 KB per connection; three Release runs gave 66.5, 77.9, and 70.7 KB | Pass |
| T4 Cost | Server CPU seconds per 1,000 requests, TLS against plain, within 5%, five interleaved trials; handshakes per second at one core | `tls_cost.py` (9f8c62b07) runs the three workloads; one 2-second trial on a busy host checked the script only | Pending: the lead's measurement |
| T5 Sanitizers | T1 and T2 clean under ASan, UBSan, and TSan | ASan+UBSan with `-fno-sanitize-recover=all`: `test_tls` 59 checks; plain docs E2E and T1 over HTTPS each 335 requests and 570 assertions, 0 failed; T2 25 of 25; no report. TSan: the same counts; no report. Before the bump, UBSan aborted every node in rapidjson (finding 6) | Pass |
| T6 reactor's `strict` | Static RSA and CBC refused, ECDHE-AEAD accepted, no tickets in TLS 1.2 or 1.3, with an RSA and an ECDSA certificate; unchanged without it | `reactor_tls_test` at e19afd9: `tls: ok`, exit 0 (`libs/reactor_tls_test.log`). It covers the RSA certificate only and reads tickets from the context's settings (finding 3) | Pass for what the test covers; the ECDSA case and an observed ticket-less handshake are owed by reactor |
| T7 The http additions | `set_idle_timeout`, `set_header_timeout`, and `on_handshake_failure` fire at their bound and reason, and change nothing when unset | `timeouts_test` at dc3be77: the idle (from the start and after a reply), header, and inactivity timeouts fire, a moving body completes, nothing fires unset; exit 0. `tls_asio_test`: `protocol`, `certificate`, and `deadline` reported; exit 0 (`libs/`). `version` and `cipher` are covered end to end by T2, not here; no unset-callback case (finding 4) | Pass for what the tests cover; the missing reasons and the unset callback are owed by http |

Under the sanitizers T3 is informational (T5 asks for T1 and T2), and holds 12 of 12 in both:
the unread reply closes 59.9 s (ASan+UBSan) and 59.8 s (TSan) after the server's last read
or write, 60.7 s and 62.6 s after the client's last write.

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
python3 -I harness/bodydiff.py old/plain.json plain.json  # T1's bodies against another build's capture
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
| C4 | Idle bound after the handshake; body and reader residuals bounded by inactivity; the cap checked against the file limit | Pass (T3 on Release: idle 75.0 s, stopped body 60.0 s, unread reply 60.0 s from the server's last byte, the fd-limit refusal; the same under both sanitizers) | `src/manager.cc:1010-1012`; `src/tls.cc` (`tls_setup`, the `getrlimit` check); `harness/tls_bounds.py` |
| C5 | A `protocol` reason; connection-cap refusals reported | Pass: T2's metric cases, T3's `handshakes`, `deadline`, and `cap` counts, and `test_tls` | `src/tls.cc` (`tls_refused`, `TlsWatch::tick`); `src/manager.cc:995-1006, 1124-1130`; `tests/test_tls.cc`; `harness/tls_check.sh` metric cases; `harness/tls_bounds.py` |
| C6 | The context built before `demote()`; expiry checked daily, with a metric | Pass: `tls_setup()` after `detach()` and before `demote()`; the daily schedule in `test_tls`; the 3-day certificate warns and its seconds to expiry show in T2 | `src/main.cc:1024, 1122, 1130`; `src/tls.cc` (`tls_check_expiry`, `TlsWatch::tick`); `src/manager.cc:1124-1130, 1793`; `tests/test_tls.cc` |
| C7 | TLS 1.2 session cache decided and tested; `strict` recorded with Detent | Pass: `SSL_SESS_CACHE_OFF` under `strict`, checked by T6's test; Detent's ledger lists the option | reactor `reactor_tls.h:70-75`, `test/tls_test.cc:162`; Detent `docs/DEPENDENCY_UPGRADES.md:57` at 68ab6c6 (line 77 at its head) |
| C8 | UBSan fails on undefined behavior; renegotiation over TLS 1.2; T1's CA and SAN; T4's CPU source | Pass: UBSan aborts on the first report (it stopped every node before the rapidjson bump, finding 6); `s_client -tls1_2` forces 1.2 and renegotiation is refused; the probe uses `--cacert` and newman `--ssl-extra-ca-certs`; the SAN is `localhost` and `127.0.0.1`; CPU from `/proc/<pid>/stat` and `ps -o cputime` | `.github/workflows/ci.yml:100, 106`; `harness/tls_check.sh:158`; `harness/e2e_capture.sh:48-54`; `harness/tls_certs.sh:18`; `harness/tls_cost.py:44-59` |
| SYS-13 | A key at 0640 accepted only with the group the node runs as; wider modes refused | Pass: `test_tls` (the mode table, the file path, a root start with `--gid`) on Release, ASan+UBSan, and TSan; T2's key-mode cases on all three. No test starts a node as root (finding 5) | `src/tls.cc` (`tls_key_mode_refusal`, `tls_setup`); `src/main.cc` (`demotion`, `demote`, the `tls_setup` call); `tests/test_tls.cc` (`test_key_mode_rule`, `test_setup_key_modes`, `test_root_start_with_gid`); `harness/tls_check.sh` case 6 |
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

6. Resolved (the lead's ruling: the root cause). rapidjson v1.1.0's `Stack::Reserve`
   computed `stackTop_ + sizeof(T) * count` while `stackTop_` was null, so UBSan
   (`-fno-sanitize-recover=all`) aborted every node on its first error response, from
   `SearchService::on_error` (`search_views.cc:3171`), the plain node included. Upstream
   16872af8 ("Avoid pointer arithmetic on null pointer to remove undefined behavior")
   compares `stackEnd_ - stackTop_` instead; it descends from v1.1.0, 433 commits on
   (rebased onto 2018 master, merged as #727, 3cf4f7c5), and is the earliest commit with
   the fix. `CMakeLists.txt` pins it with the reason beside the pin (e857e12f4); it builds
   with no rapidjson warning, and T1's bodies show no JSON change. Of the Kronuz
   libraries, only msgpack declares rapidjson, at 24b5e7a8 (2025), which has the fix, so
   none built alone carries v1.1.0; Xapiand's first declaration had been overriding
   msgpack's newer pin. Xapiand (16872af8) and msgpack (24b5e7a8) still disagree, and
   `pin_consistency.py` compares only `github.com/Kronuz/*` pins, so it does not see it.
7. Resolved (the lead's ruling: fix the harness). T3's unread-reply case timed the close
   from the client's last write, and the server keeps serving requests it already
   buffered after that write (60.2 s Release, 63.2 s TSan, 65.0 s ASan, measured that
   way). It now samples the server end's Recv-Q and Send-Q every 0.2 s (`netstat` on
   macOS, `ss` on Linux) and times the close from the last fall of Recv-Q or rise of
   Send-Q, the server's last byte read or written (f119ffe1e). The client's last-arrival
   time, the other reference considered, reads early (55.2 to 55.8 s): the peer's
   zero-window probes and the server's writes into its own send buffer move after the
   client's receive window closes. Both client-side times stay in the row's JSON. The
   60 s bound is unchanged.
8. Node banners print `git:` the revision their build directory was configured at, not
   the one compiled: `get_git_hash()` runs at configure time (`CMakeLists.txt:1572`). Not
   changed.

Not run: T4 (the lead's); LeakSanitizer; anything on Linux, and so CI's sanitizer jobs; C1
on Linux; C3's package builds; a node started as root; the cluster layers over HTTPS (step
1 leaves node traffic in clear).
