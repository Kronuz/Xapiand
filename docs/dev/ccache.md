# Cross-checkout ccache

`FIXTURES_PATH` embeds the checkout's absolute `oldtests/` path in the
shared generated `config.h`. That changes the header digest in every new
checkout, forcing preprocessing for translation units that include it.
The fix defines the path only on legacy test targets.

## Evidence

The baseline is Xapiand:c3f270ecc, which contains Xapiand:05960cb81. The
fix is Xapiand:573d33cd4; the full fixed builds use Xapiand:6f1a5424f,
whose product code is identical. This is x86_64 macOS, Apple Clang 17,
ccache 4.14.1, Release (`-O3 -DNDEBUG`), and LTO off. Each checkout has
independent dependency source directories cloned from the pinned sources
in the completed master build. Each arm uses a separate initially empty
cache with eviction disabled, compression on, and file cloning off.

Each full build makes 224 compilation calls. The second-checkout results
are:

| Code | Direct hits | Preprocessed hits | Misses |
|---|---:|---:|---:|
| Xapiand:c3f270ecc | 193 / 224 | 31 / 224 | 0 / 224 |
| Xapiand:6f1a5424f | 224 / 224 | 0 / 224 | 0 / 224 |

Both first checkouts record 224 cold misses. All 31 baseline fallback
logs identify the same generated `config.h` digest mismatch. The fix
eliminates 31 preprocessor runs per fresh checkout. A cached clean rebuild
also records 224/224 direct hits: the observed A/A hit-count spread is zero.
No elapsed-time,
CPU-time, power, or dollar saving is claimed.
The compilation database has 229 entries; five targets are excluded from
the default build. The earlier task's 202-TU denominator is not reused.

The two baseline headers differ only in `FIXTURES_PATH`. The direct-mode
input streams from the representative `package.cc`, `length.cc`, and
`server/mime_types.cc` probes are identical across checkouts. For the last
of those, ccache records:

```text
Mismatch for src/config.h: size 18447 != 18440
Did not find result key in manifest
Result: preprocessed_cache_hit
```

The complete build's 31 fallback logs report a header hash mismatch rather
than a size mismatch because its two checkout names have equal lengths.
`FIXTURES_PATH` has two users: `oldtests/test_patcher.cc` and
`oldtests/test_geospatial.cc`. Their targets retain the absolute fixture
location as a private compile definition.

`tests/check_config_relocatable.cmake` configures the actual template with
two fixture roots and requires identical output. It fails against the
baseline template and passes with the fix. It is also registered in CTest.
[Linux CI](https://github.com/Kronuz/Xapiand/actions/runs/43550909873)
passes at Xapiand:6f1a5424f.

## Reproduction

`cmake/ccache_build.sh` builds one configuration with at most four jobs,
records compilation-only statistics in `BUILD_BASENAME.stats`, and writes
ccache's cumulative counters and debug inputs alongside them. Run it under
`quiet light`; it requires absolute paths. `DEPENDENCY_SEED` is a completed
build's `_deps/` containing the pinned `*-src` trees. It copies those trees
into each checkout's `.scratch/ccache-deps/`. Configure compiler probes
occupy the cache but are excluded from the per-build stats log.

```sh
ROOT="$HOME/code/Kronuz/.scratch/codex"
REPO="$HOME/code/Kronuz/Xapiand"
SEED="$ROOT/idle-wakeups/build/_deps"
HELPER="$ROOT/ccache/cmake/ccache_build.sh"
git -C "$REPO" worktree add --detach "$ROOT/repro-before-a" c3f270ecc
git -C "$REPO" worktree add --detach "$ROOT/repro-before-b" c3f270ecc
quiet check
CCACHE_BASEDIR="$ROOT" quiet light ccache-before-a 30 "Baseline first checkout" -- \
  sh "$HELPER" "$ROOT/repro-before-a" "$ROOT/repro-before-a/build" \
  "$ROOT/repro-before-cache" "$SEED" release
CCACHE_BASEDIR="$ROOT" quiet light ccache-before-b 30 "Baseline second checkout" -- \
  sh "$HELPER" "$ROOT/repro-before-b" "$ROOT/repro-before-b/build" \
  "$ROOT/repro-before-cache" "$SEED" release
```

Repeat with two fresh worktrees at Xapiand:6f1a5424f and a separate cache
evidence directory. Use fresh build directories; the first discarded
fixed-arm attempt reused a cache whose `COMPRESSORS_LZ4_PINNED=ON` setting
conflicted with Xapiand's LZ4 v1.8.1 source override. That configure failure
is unrelated to the fixture-path fix and is excluded from the results.

For the regression, extract the baseline template and run the same check
against each template:

```sh
git -C "$REPO" show c3f270ecc:config.h.in > "$ROOT/baseline-config.h.in"
quiet light ccache-regression 5 "Check configuration relocation" -- \
  cmake -DTEMPLATE="$ROOT/ccache/config.h.in" \
  -DWORK="$ROOT/config-check" \
  -P "$ROOT/ccache/tests/check_config_relocatable.cmake"
# Replacing TEMPLATE with baseline-config.h.in must exit nonzero.
```

## Working set

Each configuration uses a dedicated initially empty cache with
`CCACHE_MAXSIZE=0`. Sizes are ccache's logical compressed cache size in
KiB, including compiler probes and manifests; filesystem allocation from
`du` is a different quantity. No shared cache is cleared or reconfigured. The shared cache enables
`file_clone=true`, while these isolated caches use ccache's default
`file_clone=false`; compressed footprint is therefore a distinct profile.
`ccache --show-compression` also records the uncompressed content size so
capacity planning can account for the shared cache's clone mode.

ASan+UBSan and TSan use Homebrew Clang 23.1.2 with the SDK libc++ headers
and library. Explicit `-nostdinc++ -I.../c++/v1` places the SDK C++ wrappers before
the SDK C headers and disables LLVM's default C++ include directory.
The initial attempts with LLVM's default C++ headers fail their wrapper
checks and do not constitute complete working sets. Reproduce the matched
SDK configuration as follows:

```sh
export SANITIZER_CC=/usr/local/opt/llvm/bin/clang
export SANITIZER_CXX=/usr/local/opt/llvm/bin/clang++
export SANITIZER_CXX_FLAGS="-nostdinc++ -I$(xcrun --show-sdk-path)/usr/include/c++/v1"
CCACHE_BASEDIR="$ROOT" quiet light ccache-asan 30 "ASan cache working set" -- \
  sh "$HELPER" "$ROOT/ccache-second" "$ROOT/asan-repro" \
  "$ROOT/asan-cache" "$SEED" asan
```

Repeat with `tsan` or `tests` and separate build/cache directories. The
test configuration uses Apple Clang, RelWithDebInfo, `BUILD_TESTS=ON`,
and `ASSERTS=ON`.

| Complete configuration | Compressed cache, KiB | Uncompressed content, MiB |
|---|---:|---:|
| Release | 21,328 | 71.1 |
| ASan + UBSan | 159,644 | 500.9 |
| Test targets | 166,624 | 521.6 |

TSan is paused before compilation to let the pending isolated measurements
run. Its complete footprint and the final size recommendation remain to
be collected. The three completed profiles perform no cache cleanups.

Local raw evidence is under `$ROOT/ccache-evidence/`: `full-before.log`,
`full-before/`, `full-after-clean.log`, `full-after-clean/`, and
`working-*.log`/`working-*/`. The original exact commands are retained in
`full-release.sh` and `working-set.sh`; they use independently cloned
`.deps/` directories rather than the committed helper's `.scratch/` path.
