# Cross-checkout ccache investigation

Status: partial. The reported 31 misses out of 202 translation units after
Xapiand:05960cb81 are the starting observation, not a reproduced result.
The fixture-path fix has a failing-before regression. Full-build hit counts
and a replacement cache-size recommendation remain unmeasured.

## Environment

The investigation starts at Xapiand:c3f270ecc on x86_64 macOS with Apple
Clang, ccache 4.14.1, Release, LTO off, and the system libc++. Two fresh
worktrees use the same commit and existing dependency source directories.
This controls dependency versions but does not reproduce independent
FetchContent directory layouts. The current compilation database contains
230 entries, so its denominator differs from the earlier 202-TU report.

The shared cache snapshot reports a 15.0 GB limit, 15.0 GB occupied, and
5,535 cleanups. Its cumulative totals span unrelated builds; they cannot
measure one Xapiand configuration's working set. The filesystem reports
52 GiB available at the start. No shared cache statistics, settings, or
entries are reset or removed by these probes.

## Cause and fix

At Xapiand:c3f270ecc, the two generated `src/config.h` files differ only in
`FIXTURES_PATH`: one contains the first checkout's absolute `oldtests/`
path, and the other the second checkout's. Sizes are 18,440 and 18,447
bytes. This unused macro changes the file digest even for production
translation units that do not use legacy fixtures.

The second-checkout ccache log for `src/server/mime_types.cc` records:

```text
Mismatch for src/config.h: size 18447 != 18440
Did not find result key in manifest
Result: preprocessed_cache_hit
```

The direct-mode input streams for all three probes are byte-identical;
this is a manifest header mismatch rather than a compiler-argument
mismatch. At Xapiand:573d33cd4, `FIXTURES_PATH` moves out of `config.h.in`
and into the legacy test targets' private compile definitions. Its only
users are `oldtests/test_patcher.cc` and `oldtests/test_geospatial.cc`.
Production configuration no longer carries the checkout's fixture path.

`tests/check_config_relocatable.cmake` configures the actual template with
two fixture roots and requires identical output. It fails against the
baseline template and passes against the fix, under `quiet light`:

```sh
ROOT="$HOME/code/Kronuz/.scratch/codex"
git -C "$ROOT/ccache" show c3f270ecc:config.h.in \
  > "$ROOT/ccache-evidence/baseline.h.in"
quiet check
quiet light codex-ccache-regression 5 "Check configuration relocation" -- \
  cmake -DTEMPLATE="$ROOT/ccache/config.h.in" \
  -DWORK="$ROOT/ccache-evidence/fixed" \
  -P "$ROOT/ccache/tests/check_config_relocatable.cmake"
# The same command with baseline.h.in must exit nonzero.
```

## Probe

The probe compiles `src/package.cc`, `src/length.cc`, and
`src/server/mime_types.cc` from each checkout through ccache. It uses an
isolated cache and writes debug inputs and per-checkout stats logs.
This is a diagnostic subset, not a full-build hit-rate measurement.

```sh
ROOT="$HOME/code/Kronuz/.scratch/codex"
quiet check
quiet light codex-ccache-config 10 "Configure two checkouts" -- \
  sh "$ROOT/ccache-evidence/configure.sh"
quiet light codex-ccache-probe-a 5 "Collect first checkout cache inputs" -- \
  /usr/local/bin/python3 "$ROOT/ccache-evidence/probe.py" ccache
quiet light codex-ccache-probe-b 5 "Collect second checkout cache inputs" -- \
  /usr/local/bin/python3 "$ROOT/ccache-evidence/probe.py" ccache-second
```

Local raw evidence is in `$ROOT/ccache-evidence/`: `configure.log`,
`probe-a.log`, `probe-b.log`, `ccache.stats`, `ccache-second.stats`, and
`debug/`. The scripts record the exact dependency-source overrides and
compilation-database commands. `CCACHE_DEBUG=1` writes the
`.ccache-input-text` files; `CCACHE_BASEDIR=$ROOT` and
`CCACHE_NOHASHDIR=true` normalize checkout paths without modifying the
compiler commands in CMake. The first checkout produces three cold misses. Before the fix, the second
checkout returns two direct hits and one preprocessed hit.

## Results

| Code | Second-checkout direct hits | Preprocessed hits | Cold first-checkout misses |
|---|---:|---:|---:|
| Xapiand:c3f270ecc | 2 / 3 | 1 / 3 | 3 / 3 |
| Xapiand:573d33cd4 | 3 / 3 | 0 / 3 | 3 / 3 |

The fixed checkouts produce identical generated config headers. Each arm
uses a separate initially empty cache, and no source output bytes change.
The after arm is recorded in `probe-after.log` and `after/`:

```sh
quiet light codex-ccache-after 5 "Compare fixed checkouts" -- \
  env CODEX_CCACHE_EVIDENCE=ccache-evidence/after \
  /usr/local/bin/python3 "$ROOT/ccache-evidence/probe.py" ccache ccache-second
```

These counts establish the cause for one translation unit in the subset;
they do not establish that every one of the original 31 misses has this
cause. No elapsed-time or CPU-efficiency result is measured.

## Remaining evidence

Release, ASan, TSan, and test-target working sets each require a full build
into a dedicated initially empty cache with eviction disabled, recording
compressed cache bytes and files before discarding build output. Distinct
configuration caches avoid attributing another configuration's cleanup to
this one. Shared-cache cumulative bytes and cleanup counts do not answer
that question.

The full 230-entry baseline, independent dependency-layout comparison,
second full build, and four configuration sizes remain unmeasured.
No before/after speed claim is made; A/A timing spread is unavailable.
Keep the existing cache limit until the working sets and disk headroom
support a measured replacement.
