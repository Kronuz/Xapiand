# Coverage

How to measure source-based coverage of `src/` and what the tests in `tests/test_coverage_*.cc` add. The targets (T13) are
in [TESTING.md](../../TESTING.md#coverage); this page is the how.

## Running it

```sh
quiet light cov 150 "coverage" -- harness/coverage.sh OUTDIR          # build-cov, then ctest, corpora and E2E
WORKLOADS=ctest harness/coverage.sh OUTDIR                            # unit tests only
harness/coverage.sh report OUTDIR                                     # the table again from OUTDIR's merged profile
```

`harness/coverage.sh` builds the whole tree with `clang -fprofile-instr-generate -fcoverage-mapping` into `build-cov`, runs
the workloads with one `LLVM_PROFILE_FILE` pattern, merges the profiles and prints each area's lines and branches beside
its target. It writes, in OUTDIR:

| File | Holds |
|---|---|
| `coverage.txt` | The per-area table |
| `coverage-files.txt` | Every `.cc` file's lines and branches (`COV_HEADERS=1` adds headers) |
| `coverage-uncovered.txt` | Each file's uncovered lines and unexecuted branches, from the lcov export |
| `coverage.json`, `coverage.lcov` | The `llvm-cov export` of the merged profile |

`STRICT=1` exits 3 when an area is below its target. The E2E workload needs `newman` 6 on `PATH`; without it that workload
is skipped and the table undercounts the server and database areas.

An aborted test writes no profile, so a test that crashes shows no gain: read `ctest.log` first when an area looks too low.

## One test at a time

```sh
harness/coverage_one.sh serialize src/serialize.cc                    # builds tests/test_serialize.cc instrumented, runs it
```

prints for each named file its lines and branches and the uncovered ones. `REPEAT=20` runs the test 20 times, fails when any run does and prints how many libc++ copies the binary links; that is how an exit-time abort is checked. It builds only that test into
`build-cov-TEST`, so a coverage test iterates in minutes rather than a full build.

## One C++ runtime

Every instrumented binary must load one `libc++` and one `libc++abi`. CMake's `find_library(c++)` finds the SDK's runtime
while the instrumented compile uses LLVM's headers and `-L`; a binary that links both throws in one runtime and catches
in the other (an `InvalidArgument` escaping a `catch`) and locks a mutex the other runtime destroyed (a
`std::system_error: mutex lock failed` at exit). `harness/coverage.sh` names LLVM's libraries
(`-DLIBCPP_LIBRARIES`, `-DLIBCPPABI_LIBRARIES`, an rpath on `$LLVM/lib/c++`) and `one_runtime()` fails the build, with the
binary's name, when `otool -L` lists two. `coverage_one.sh` links the same way. Add the check to any new coverage script
that builds its own binaries.

## The coverage tests

Each file is one `TEST_MAIN` binary registered by name in ctest (`ctest -R test_coverage`):

| Test | Covers |
|---|---|
| `test_coverage_serialize.cc` | `src/serialize.cc` |
| `test_coverage_fieldparser.cc` | `src/field_parser.cc` |
| `test_coverage_y2j.cc` | `src/y2j/` |
| `test_coverage_aggregations.cc` | `src/aggregations/` |
| `test_coverage_lua.cc` | `src/lua/` |
| `test_coverage_geospatial.cc` | `src/geospatial/` |
| `test_coverage_reader_registry.cc` | the reader registry's bytes per open index |

A test pins today's behavior, including behavior that looks wrong; the product bugs it found are in the quests
`serialize-validation` and `geo-correctness`, and each pinned check names its finding so the fix flips one line.

## Mutants

`harness/coverage_mutants.sh MUTANT` applies one small mutation to the code a coverage test covers, builds that test and
runs it; the exit status is the test's, so a job submitted with `--fails-without` passes when the mutant is killed. It
exits 126 when the mutation does not apply, which never counts as killed.

```sh
campaign job submit --quest coverage-areas --for A1 --fails-without -- harness/coverage_mutants.sh geo-km
```

## Reader registry bytes (D90)

`test_coverage_reader_registry` prints the registry's bytes per open index: 120 B idle, 440 B at the first reader and
15,800 B for 194 readers (the old fixed registry held 12,416 B for 194 records).
