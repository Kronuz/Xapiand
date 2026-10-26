# ASan container-overflow in prometheus-cpp Summary::Observe

Verdict: false positive, caused by an uninstrumented dependency.

Cause: `FindSanitizers` appends `-fsanitize=address` to `CMAKE_CXX_FLAGS` after every
`FetchContent_MakeAvailable`, so prometheus-cpp `core` (and the other fetched libraries)
compile without ASan (`flags.make` has no `-fsanitize`). `CKMSQuantiles::insertBatch`
(`ckms_quantiles.cc:122`, `sample_.emplace` in the middle of the vector) shifts elements with
`memmove`; the ASan interceptor checks the range against the vector's container annotations,
which the uninstrumented code never keeps in step. `Summary::Observe` holds its mutex, so there
is no race, and the indexing in `insertBatch` is in bounds.

Fix (CMakeLists.txt): the `find_package(Sanitizers)` block moves above `include(FetchContent)`,
so every fetched library that compiles sources gets the sanitizer flags (prometheus `core` and
logger carry `-fsanitize=address`; storage and cluster are header-only). Checked in `flags.make`
of fresh ASan and TSan configures: all 26 fetched-dependency targets carry the flag.

Evidence (`uafhunt.sh`, FLUSH hammer, Debug ASan, strict hold, default `ASAN_OPTIONS`):
- Control (offload-bridge ASan binary): 2 of 4 runs abort at round 8, container-overflow in
  `__constexpr_memmove` under `CKMSQuantiles::insertBatch`; the earlier recheck was 2 of 2.
- Fixed build: 0 reports in 2 runs x 30 rounds (and 0 in 14 and 30 rounds with the first,
  per-target variant).
- `xapiand_test_blocking_guard` is not on master, so the guard test was not run.

TSan (separate fresh Debug build): `xapiand_test_endpoint`, `xapiand_test_engine_shard` and
`xapiand_test_debounce_or_run` exit 0 with 0 ThreadSanitizer reports; the newly instrumented
libraries surface nothing there. The server workload was not run under TSan.
