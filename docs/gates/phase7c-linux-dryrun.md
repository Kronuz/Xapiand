# Phase 7c gate rows, Linux x86-64 dry run

Xapiand `81c311799` (master, bundle shipped by scp) on one c7a.4xlarge (16 vCPU, 30 GiB, Ubuntu 24.04, lease
`strata-gate7c-dry`, 0.97 h, about $0.80). Clang 20 + libc++ 20 (Ubuntu's clang 18 cannot build the logger's
`std::format` use; the pinned RapidJSON needs the two local edits `docs/dev/testing.md` names, applied in the fetched
copy). Release (LTO) build 2.5 min; ASan+UBSan 4.6 min; TSan 2.7 min. Dry run: reduced sizes, one trial each, no
quiet hold (own machine), numbers are smoke values, not gate values.

Harness repairs made on the remote clone only (none committed): `ulimit -n 1048576` (the default 1024 refuses
`--tls-max-connections 1024`), `/usr/local/bin/python3` symlink, readiness probes accepting the 400 below, and
`strata_check` built per `docs/gates/phase7a.md` for `engine_crash_check.sh`.

## Rows

| Row | Status | Time | Numbers | Blocker |
|---|---|---|---|---|
| Release build | runs | 2.5 min | 0 warnings-as-errors; needs clang 20 and patched RapidJSON | Clang 18 fails (`std::format` of `stringified` in fs.cc); RapidJSON `document.h:319`, `stack.h:117` fail to build unpatched |
| Unit (ctest, Release) | pass | 12 s | 32 of 32 | none |
| Unit, ASan+UBSan | 1 fail | 53 s | 31 of 32 | `test_text`: `test_body_depth_per_format` (`test_text.cc:576`) asks msgpack for a 0x2000000008-byte array; ASan reports out-of-memory and aborts. Passes with `allocator_may_return_null=1` and in Release |
| Unit, TSan | 4 fail | 149 s | 28 of 32 | `test_node`, `test_engine_host`, `test_pool`: data race in `pthread_cond_destroy` vs `pthread_cond_signal`/`free` on `engine::BlockingScheduler` (`src/engine/host.h:214` vs `:221`; created in `sync_wait`, `sync.h:82`, destroyed by the thread-local destructor while another thread's `task::event::set()` posts to it; repro: `ctest -R test_node` in a TSan build). `test_text`: the same OOM as ASan |
| T1 `async_check.sh --strict` | pass | 1 s | exit 0 | none |
| T1 `--strict --owed` | fails as expected | 2 s | 254 hits: allow 218, owed 23, compiled_out 13, unclassified 0 (owed: D4 4, D5 2, D7 2, X1 15) | carried condition: the owed rows are undecided |
| T1 thread census (`stall_load --census`, mid-loaded phase) | pass | in suite | Xapiand:MAIN 17, W 16, FS 9, WL 8, LOG 1; 51 threads; offenders none | none |
| T2 docs E2E (Release) | pass | 73 s | GREEN | none |
| T2 docs E2E (ASan) | pass | 77 s | GREEN, 0 sanitizer reports | none |
| T2 `stored_stream.sh` | pass | 10 s (ASan 10 s) | under, over and 2x cap byte-equal; 4 parallel; 413/400 refusals | needs the python3 symlink |
| `restore_stream_check.sh` | pass | 17 s (ASan 40 s) | 20,000 NDJSON, 5,000 MsgPack, chunked, bad line, mid-stream cut (14,354 stay), kill -9 | needs the python3 symlink |
| T3 `wal_recovery_check.sh` | pass | 4 s | 50 of 50 | none |
| T3 `engine_crash_check.sh --reactors 2` | pass | 18 s | A: 50 of 50; B: 12,141 acked, 12,141 recovered; D: 2 writers, 23,313 recovered, prefix property holds; strata check ok on all shards | needs `strata_check` built by hand (no CMake target) |
| Shutdown (`graceful_shutdown_check.sh`, `signal_check.sh`) | pass | 18 s, 3 s | all scenarios | none |
| Path boundary | pass | 12 s | all | none |
| CLI roundtrip | pass | 4 s | dump/restore exit 0, sha256 equal | python3 symlink |
| TLS (`run_all.sh tls`) | 2 fail | 91 s | newman 6.2.3 HTTPS suite ran; 2 `tls_check.sh` checks fail | the 400 on `GET /` below (both checks expect 200 from `/`); every refusal check passes |
| T4 reactor lag | blocked | n/a | p99 lag not readable | `GET /:metrics` answers 400 (below), so `stall_load.py` gets no lag histogram, no in-flight or demoted peaks |
| T5 light p99 (`stall_load --suite --census --no-assert`, 100,000 accounts, 15 s phases) | runs, not gateable | 299 s | saturating light rate 13,600/s; unloaded at 6,800/s: p50 0.46 ms, p99 26.9 ms; loaded: p50 2.9 s, p99 5.0 s; same-index: p50 3.9 s, p99 6.1 s; 0 503, 0 errors | The load generator is one Python process (generator bound above 3,200/s, unloaded p99 rises from 1.9 ms to 26.9 ms); loaded starvation matches the pre-7c macOS baseline (`docs/perf/phase7c-baseline.md`: p50 24 s) |
| T8 overload | runs, not gateable | in suite | overload-light 4x: p50 15.5 s, p99 24.5 s, 0 503; overload-heavy 4x: 1,082 heavy searches/s closed loop, light p99 9.8 s, 0 503 | 0 503s at 4x: the row wants a non-zero count; peaks unreadable (no metrics); no `test_readers_cap` run (not in the ctest set) |
| T9 cancellation | not run | n/a | n/a | no harness; functional tests are not written |
| T10 Lua bounds (`lua_bounds.py`, Release) | 34 bounds ok, then fails | 82 s | print flood 204 in 0.20 s; light GET alone p99 0.13 ms; beside 4 long scripts p99 0.83 ms; beside 4 library loops p99 1.10 ms; beside 16 flooding clients p99 28.3 ms (max 58 ms), 3,794 answered, all 400 | The foreign-script phase starts a node with `--http-servers 1`, which X6 removed, so it never comes up (`lua_bounds.py:117`). Also its liveness probes expect 200 from `GET /` |
| T10 Lua (ASan) | times out | 52 s | 2 ok | request timeout under ASan on an early case |
| T11 `profile_light.sh` | blocked | n/a | n/a | uses `sample` (macOS only) and needs a HTTP_TRACE_HOP baseline build |
| T12 `profile_bulk.sh` | blocked | n/a | n/a | macOS `sample`; the pre-7c binary is not built |
| T12 `loadtest.py` (20,000 docs x3 trials, 16 clients, 10 s) | runs | 66 s | index 1,900 / 1,862 / 1,868 docs/s; queries 7,844 / 7,251 / 7,074 QPS, p50 1.9 to 2.1 ms, p99 4.5 to 5.4 ms | no pre-7c Linux baseline; generator-bound |
| T7 `engine_mixed_load.py`, 90 s, ASan | pass | 97 s | 39,905 docs, model equals dump, 0 failures, 0 sanitizer reports | `--term-at` does not exist, so no SIGTERM-at-300 s check; 600 s not run |
| T7, TSan, 90 s | pass | 104 s | 29,340 docs, model equals dump, 0 failures, 0 reports | same |
| T6 reactor faults | not run | n/a | n/a | needs `--index-size 2xram` on a gate VM and a `getrusage` probe |
| T13 UBSan (with ASan), TSan builds | build ok | 4.6, 2.7 min | see unit rows | coverage, restore corpora, 10,000-open-index cost not run |

## Findings

1. **`GET /` and `GET /:metrics` answer 400** ("Index path '' is empty or outside the data directory", from
   `4ec3d6d03`). Not Linux-specific. It breaks `run_all.sh`'s `wait_http` (smoke, load, stress, bench all fail on a
   healthy node), `lua_bounds.py`, `tls_check.sh`, and `stall_load.py`'s lag and peak scrape, so T4, T5 and T8 cannot
   read their lag columns. Repro: start a node, `curl -i localhost:8880/`. Either the router must serve root
   commands or the harness must probe elsewhere; decide before the gate run.
2. **TSan race on `engine::BlockingScheduler`** (condition variable destroyed by a thread-local destructor while
   another thread signals it); repro above. Possibly benign ordering, but unannotated.
3. **Harness portability**: `/usr/local/bin/python3` is hard-coded in `cli_roundtrip_check.sh`,
   `stored_stream.sh`, `restore_stream_check.sh`, `profile_light.sh`; `profile_light.sh` and `profile_bulk.sh` need
   macOS `sample`; the default fd limit of 1024 refuses TLS startup; `engine_crash_check.sh` needs a hand-built tool.
4. **Toolchain**: clang 18 does not build; clang 20 with unpatched RapidJSON does not build.
5. **Not yet runnable on any host**: T9 harness, `--term-at`, T6 probe, T11 trace build, a pre-7c baseline binary.
6. **Flood bound on Linux**: 16 flooding clients leave light GETs at p99 28.3 ms, against 13 ms recorded on the
   Mac (`road-to-release.md`); the row's "near 20 ms plus the host's own" holds loosely.
