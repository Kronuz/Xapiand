# Testing & Stressing Xapiand

How to test **and** stress every layer of Xapiand, what each layer proves, and how
to run it. The tools live in `harness/`; one command runs them all:

```sh
harness/run_all.sh                 # smoke + e2e + tls + recovery + load + stress + bench
harness/run_all.sh smoke recovery  # just a subset
```

Everything here drives the built binary (`build/bin/xapiand`) over its real HTTP
port. There is no mocking: a "green" run means the actual server did the
work. Build first:

```sh
(cd build && cmake .. && make xapiand -j4)
```

---

## The layers at a glance

| Layer | Tool | What it proves | Needs |
| --- | --- | --- | --- |
| **smoke** | `run_all.sh smoke` | a single node indexes and searches at all | nothing |
| **unit** | `ctest` | C++ units (parsers, serialise, wal, query, ...) in isolation | `-DBUILD_TESTS=ON` + GTest |
| **functional E2E** | `run_all.sh e2e` | the whole documented HTTP API still behaves (doc-embedded assertions) | `newman` |
| **TLS** | `run_all.sh tls` | the E2E suite over HTTPS, and every TLS refusal names its reason (`tls_check.sh`) | a `-DTLS=ON` build, `newman` 6 |
| **recovery** | `wal_recovery_check.sh` | WAL crash recovery: uncommitted writes survive a `kill -9` via open-time replay | nothing |
| **load** | `index_fortune` | bulk indexing of many docs | nothing (falls back if no `fortune(6)`) |
| **stress / soak** | `stress_fortune` | correctness under sustained concurrency | nothing |
| **benchmark** | `loadtest.py` / `es_loadtest.py` | index throughput + query QPS/latency (vs Elasticsearch) | a dataset (bundled) |

The HTTP E2E is the workhorse for correctness. See `docs/_docs/benchmarks.md` for
recorded benchmark results.

---

## smoke

The cheapest sanity check: boot one node, `PUT` a document with
`?commit=true`, search for a term in it, expect one hit. No baseline, no
dependencies. `run_all.sh` runs it first so a broken binary fails fast.

```sh
harness/run_all.sh smoke
```

---

## Unit tests (ctest)

C++ unit tests live in `tests/` (active) and `oldtests/` (a large legacy GTest
suite: `boolparser`, `compressor`, `serialise`, `wal`, `query`, `geospatial`,
`msgpack`, `url_parser`, ...). They're only built when you configure with
`-DBUILD_TESTS=ON`, which needs GoogleTest:

```sh
cmake -S . -B build -DBUILD_TESTS=ON      # (GTest must be findable)
cmake --build build --target check         # builds + runs via ctest
# or, after building:
(cd build && ctest --output-on-failure)
```

`run_all.sh unit` runs `ctest` and **skips** (doesn't fail) if no tests are
configured, telling you to reconfigure. The `oldtests/` suite is wired but marked
legacy in `CMakeLists.txt`; re-enable modules there as they're brought current.

### Sanitizers locally, and the T10 mixed load

The `SANITIZE_*` CMake options did not instrument the local build; use raw flags with Homebrew LLVM. On macOS the SDK's `usr/include` must not reach the compiler as an `-I`, so point the two cache entries
that name it at a directory of symlinks:

```sh
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_TESTS=ON -DLTO=OFF \
  -DCMAKE_C_COMPILER=/usr/local/opt/llvm@22/bin/clang -DCMAKE_CXX_COMPILER=/usr/local/opt/llvm@22/bin/clang++ \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer -g1" \
  -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -g1" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined -L/usr/local/opt/llvm@22/lib/c++ -Wl,-rpath,/usr/local/opt/llvm@22/lib/c++" \
  -DZLIB_INCLUDE_DIR=$PWD/sdkinc -DUUID_INCLUDE_DIR=$PWD/sdkinc      # sdkinc: symlinks to zlib.h, zconf.h, uuid/
```

For TSan use `-fsanitize=thread`. RapidJSON is pinned to the master commit Kronuz/msgpack uses (v1.1.0 does not build with clang 20 and
aborts under UBSan), so no local edits are needed.

On Linux the CI sanitizer jobs use gcc-13 and the CMake options (`-DASAN=ON -DUBSAN=ON`, `-DTSAN=ON`). CMake builds against libc++ on Linux
only when `<version>` defines `__cpp_lib_format` (libc++ 19 and later), so Ubuntu 24.04's clang 18 builds with libstdc++ even with
libc++-18-dev installed; clang 20 with libc++-20-dev uses libc++.

Then:

```sh
(cd build-asan && ASAN_OPTIONS=detect_leaks=0 ctest -j3)
harness/engine_e2e.sh build-asan/bin/xapiand OUT                         # the docs E2E on a fresh node
TSAN_OPTIONS=log_path=OUT/san python3 harness/engine_mixed_load.py build-tsan/bin/xapiand OUT --secs 600   # T10
harness/stored_stream.sh build-asan/bin/xapiand                          # T2's upload rows: 2x the body cap, one byte over, one just under
harness/restore_stream_check.sh build-asan/bin/xapiand                   # RESTORE streamed, chunked, with a bad line, and cut off
```

`stored_stream.sh` uploads `stored` blobs of 2x the buffered-body cap, one byte over it and one just under it, and reads each
back byte-equal, then four uploads at once, the refusals (past the record limit 413, a POST without an id 400, a big JSON body
413) and a client that leaves mid-upload (no volume stays). `restore_stream_check.sh` restores 20,000 NDJSON documents, a MsgPack
stream, a chunked body and a stream with a bad line, counts each back, and cuts a stream off mid-way.

`engine_mixed_load.py` runs writers (PUT, RESTORE batches, replace, DELETE against a model) and searchers on one single-shard
strata index for the stated time, then compares the final `DUMP` with the model; it exits 0 only when nothing failed and the
sanitizer wrote no report. `xapiand_fuzz_text SECONDS [SEED]` and `xapiand_fuzz_body SECONDS [SEED]` are built with the tests;
the four libFuzzer targets (`fuzz_strata_document`, `fuzz_strata_pattern`, `fuzz_index_path`, `fuzz_index_pattern`) build with `-DFUZZ=ON` or by hand
(`clang++ -fsanitize=fuzzer,address,undefined -Isrc tests/fuzz_X.cc src/engine/strata/X.cc`).

---

## Functional E2E (the docs are the test suite)

Xapiand's documentation (`docs/_docs/*.md`, `docs/tests/*.md`) is a runnable HTTP
test suite: `docs_to_postman.py` turns the fenced request/response blocks into a
Postman collection, and `newman` runs it against a live node. This is the primary
correctness net for the API surface (schemas, query DSL, ranges, restore, ...).

```sh
harness/run_all.sh e2e          # boot a node, run the collection, check assertions
```

**The docs are the spec, and each example carries its own expected result** — commented
right next to the request (inside a `{% comment %}` block, so it never renders) as
`pm.test(...)` assertions that check the meaningful thing that example demonstrates: a
search asserts its `count` / `hits`, an aggregation asserts its bucket values or
ordering, a `GET` asserts the retrieved fields, a write asserts its status. Any request
that needs nothing more than "it worked" gets a default **synthesised by the generator**:
`pm.response.to.be.success` (2xx), or a specific `pm.response.to.have.status(NNN)` when
the block declares a `status: NNN` marker — used for the deliberate error/edge demos
(unimplemented `CLOSE`/`OPEN` → 501, placeholder paths → 404, invalid-input → 400). So
**every** request is verified, and the whole expected result lives in the `.md`.

The gate (`e2e_assert.py`) is **GREEN only when every documented assertion passes and
every request is covered** — no response-body baseline, no allowlist, no skipped tests.
If a documented assertion fails, the docs and the server disagree, and that is a red
build: fix the code, or fix the doc's stated expectation. Nothing is captured, distilled,
drifted, or stored per machine, so the check runs anywhere — including CI — straight from
the docs. The collection is regenerated from the docs on every run, so a doc or generator
change can never be masked by a stale cached collection.

When behaviour changes: update that example's assertion in its `.md` to the new expected
result (a reviewable one-line diff, right where the request is). Compare floating-point
results with `to.be.closeTo(value, delta)`, never exact `equal`, so the check stays
portable across platforms.

For an on-demand **response-body** comparison between two builds (deeper than the
assertions — catches serialization changes), capture a reference report from another
binary and diff it with the optional explorers `e2e_check.sh` / `e2e_diff.py` /
`bodydiff.py`. These are investigation aids, not the committed gate.

### Text analysis differential

The doc suite barely touches text analysis, so `harness/analysis_diff.py` indexes a mixed corpus under
about 130 analysis configurations (language, stem and stop strategies, positions, weight, CJK n-grams), then
records what searches find: for every word of the corpus, and every pair of adjacent words as a phrase, the
ids of the documents that match. Run it against two builds on separate data dirs and diff the two JSON
files; any difference is a change in how text is analysed at index or query time.

```sh
python3 harness/analysis_diff.py 8891 base-analysis.json         # node on :8891
```

### Over HTTPS

`run_all.sh tls` runs the same collection over `https://` against a node started with a
throwaway CA and certificate (`tls_certs.sh`), and must report the same assertion count as
over plain HTTP. `e2e_capture.sh` takes `SCHEME`, `CA` (newman's `--ssl-extra-ca-certs`,
newman 6 or later), `XAPIAND_FLAGS`, and `PORT`; `docs_to_postman.py` takes the base URL
from `XAPIAND_URL`. The layer then runs `tls_check.sh`, which pairs each refusal with an
accept case that differs in one variable: an untrusted CA, plain HTTP to the TLS port, a
raw TLS 1.1 ClientHello, a static-RSA suite, a forced renegotiation, and startup with an
expired, not-yet-valid, mismatched, or group-readable certificate or key.

---

## Recovery — `wal_recovery_check.sh`

The durability net for the write-ahead log. Every write is appended to the per-database
WAL as it happens, *before* the debounced commit (the timed committer only forces
at ~8-10s). So a crash after some uncommitted writes must
be recoverable: reopening the data dir replays the WAL and restores every one of them.

The check makes that concrete: start a node, `PUT` N docs **without** `?commit=true`
(they live only in the WAL), `kill -9` the node within ~1s (a hard crash, well before any
timed commit), restart on the same data dir, and assert all N docs come back. It also
greps the restart log for the replay line (`Read and execute operations WAL`) as positive
proof the recovery went through the WAL rather than a lucky early commit.

```sh
harness/wal_recovery_check.sh            # 50 docs (default)
NDOCS=500 harness/wal_recovery_check.sh  # more writes
```

This is the modern, real-architecture equivalent of the retired `oldtests/test_wal.cc`,
which drove the long-gone 2019 `DatabaseQueue`/`Database` API by hand; here we exercise the
actual HTTP → WAL → crash → open-time-replay path end to end. The storage engine underneath
(`Kronuz/storage`, the append-only checksummed volume format the WAL is built on) has its
own fault-injection suite (torn writes, truncation, bit rot, a corrupt size field, and the
guarantee that records committed before any damage stay readable) — see that repo's
`test/test.cc`.

Related probes: `graceful_shutdown_check.sh` and `signal_check.sh` (signal handling /
clean shutdown).

---

## Load — `index_fortune`

A quick functional bulk-load: `PUT` a range of documents built from random
"fortunes" (uses `fortune(6)` if installed, otherwise a small built-in corpus, so it
runs anywhere).

```sh
harness/index_fortune 'http://localhost:8880/fortune?commit=true' 1 1000
```

Good for populating an index fast and eyeballing write behaviour; not a measured
benchmark.

---

## Stress / soak — `stress_fortune`

Sustained concurrency with read-back verification: N workers loop `PUT` a document
into one of M databases, then `GET` it back and check the field round-trips. It
prints one status char per op so you can watch it live, and a summary (ops, errors
by kind, rate) at the end. Unlike the benchmark, this is the "does it stay up and
stay correct under load" net.

```sh
harness/stress_fortune --target localhost:8880 --workers 50 --databases 20 \
    --duration 30 --commit
harness/stress_fortune --ops 100000 --workers 200          # op-count instead of time
```

Methodology note: each worker owns a disjoint id band (`worker*docs_per_db + i`), so
no two workers ever touch the same document — otherwise a concurrent write would race
the read-back and report a bogus mismatch. A non-zero exit means real failures
(hard errors or a verified value that didn't round-trip); eventually-consistent 404
read misses are not counted as errors. This is the dependency-free rewrite of the
original `contrib/bin/stress_fortune`.

---

## Benchmarks

`loadtest.py` bulk-loads a dataset (the bundled `accounts` set, repeated) and then
hammers a fixed query mix from many threads, reporting index docs/s and query
QPS/latency percentiles:

```sh
harness/loadtest.py --target localhost:8880 --dataset docs/assets/accounts.ndjson \
    --replicate 20 --concurrency 16 --duration 5 --trials 2 --datadir /tmp/xap_bench
```

`es_loadtest.py` runs the same query mix against Elasticsearch for a side-by-side
(different substrate — read the caveats in `docs/_docs/benchmarks.md`). `perfdiff.py`
diffs two `loadtest.py` result JSONs.

---

## The unified runner

`harness/run_all.sh` orchestrates the layers above (it doesn't reimplement them),
starting each on its own fresh data dir, and prints one pass / fail / skip summary.
Layers whose prerequisites are missing (a baseline, the oracle binary, GTest) are
**skipped**, not failed, with a note on how to enable them.

```sh
harness/run_all.sh                 # default: smoke e2e tls recovery load stress bench
harness/run_all.sh smoke stress    # a subset
BIN=/path/to/xapiand harness/run_all.sh
```

Exit code is 0 (GREEN) only if no layer failed.
