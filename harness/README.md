# harness — differential E2E + before/after performance

A regression net for the de-vendor / inversion work. Everything compares the
current build against a **baseline build** (the baseline is the oracle), so we
don't hand-author expected outputs — we run the same inputs against both and diff.

"Green" means **parity with the baseline**, not 100% pass (the doc suite has
pre-existing aspirational examples that fail on both).

## Correctness lanes

Both consume two newman JSON reports (`newman run … --reporters json
--reporter-json-export X.json`) of the doc collection (`python3 docs_to_postman.py`):

- **`e2e_diff.py base.json ours.json`** — diffs assertion outcomes + HTTP status.
  Regression = an assertion that fails only on ours. This is what caught the
  rapidjson relaxed-JSON regression.
- **`bodydiff.py base.json ours.json`** — diffs the normalized response *bodies*
  (the assertions are shallow; this catches serialization changes the Leg 2 work
  could introduce). Volatile fields (node name, auto-ids, timestamps, storage
  offsets) are normalized out. Boot both nodes with a fixed `--name` to avoid
  spurious node-identity divergences.

## Performance lane

- **`loadtest.py`** — bulk-RESTOREs a dataset (docs/sec + per-index on-disk
  bytes) then replays a validated query mix at concurrency (QPS + p50/p95/p99),
  multi-trial for a noise band. Writes a JSON result.
- **`perfdiff.py base.json ours.json`** — direction-aware compare (higher
  docs/sec & QPS better; lower latency & disk better), flagging
  improved / regressed / within-noise. The noise band is widened by the observed
  trial spread so jitter doesn't read as regression.

## Capture a baseline + compare (the loop)

The docs E2E needs newman 6 or later first on `PATH`: newman 4 fails every request, and `e2e_capture.sh` (so `engine_e2e.sh`) refuses to run with it.

```sh
# correctness (per node, fresh data dir, fixed name)
python3 docs_to_postman.py | newman run /dev/stdin --reporters json --reporter-json-export base.json   # vs baseline build
python3 docs_to_postman.py | newman run /dev/stdin --reporters json --reporter-json-export ours.json   # vs our build
python3 harness/e2e_diff.py  base.json ours.json
python3 harness/bodydiff.py  base.json ours.json

# performance (one node up at a time; harness/loadtest.py takes --target)
python3 harness/loadtest.py --datadir <dir> --label baseline --out harness/results/baseline.json
python3 harness/loadtest.py --datadir <dir> --label ours     --out harness/results/ours.json
python3 harness/perfdiff.py harness/results/baseline.json harness/results/ours.json
```

## Baseline

`e2e_check.sh` diffs a fresh capture against a saved report of the doc suite: assertion
parity is green, and the only body divergences are the genuinely run-to-run volatile
fields (the POST auto-id and the `/:metrics` counters). `e2e_assert.py` is the gate on
its own: every documented assertion must pass.

## Known refinements

- The accounts dataset is small (docs go *inplace*), so the **zstd storage codec
  isn't exercised** — add a large-document dataset to benchmark compression.
- The exhaustive differential corpus (type×op matrix, error paths, content
  negotiation, custom verbs) is not yet generated; today the correctness lane
  reuses the 303-request doc collection. Generating that corpus is the path to
  truly exhaustive coverage (the baseline-as-oracle keeps it cheap).

## Phase 7c gate tooling

Built in step S1 (`docs/design/phase7c.md`, section 5); the pre-7c numbers are in
`docs/perf/phase7c-baseline.md`. Run the timing tools under `quiet strict` (or `exclusive`).

- **`stall_load.py`** -- light open-loop latency on `accounts` (1,000,000 documents) under heavy
  load: exhaustive aggregations, wildcard and phrase searches, and a RESTORE stream into
  `restore-target`. Latency runs from each request's scheduled send time; the achieved rate is
  asserted within 5% of the target. `--phase unloaded|loaded`, `--restore-into accounts` (Q6's
  cost), `--saturate`, `--overload light|heavy N`, `--dry-run`. JSON out: light p50/p99/p99.9,
  per-loop lag deltas from `:metrics`, 503 and error counts, segments and merge backlog, RSS.
- **`thread_census.sh PID`** -- threads by name prefix (T1); `--allow` fails on any other prefix.
- **`async_check.sh`** -- every blocking primitive on a reactor-reachable path in one of four
  classes (D14, T1). `--strict` fails on an unclassified hit; `harness/async_allowlist.tsv` holds
  the rows (a new hit needs a row naming its reason or the decision that removes it).
- **`removed_options_check.sh BIN`** -- the thread-count options D15 removed (`--committers`, `--fsynchers`,
  `--http-servers`, `--http-clients`, `--bulk-preparers`, `--bulk-indexers`) are unknown arguments and not in `--help`.
- **`profile_light.sh`** -- CPU per light request by group (offload hop, scheduler, engine, HTTP)
  from `sample` (macOS) or `perf` (Linux), and the hop's wall time from an http `trace/hop` build
  (`-DHTTP_TRACE_HOP`); `--compare PRE POST` is T11. `profile_bulk.sh` samples the same way.
- **`perf_sample.py PID SECS OUT`** -- `sample(1)` for Linux: records PID with perf (cpu-clock, DWARF unwinding) and writes
  `sample`'s call-graph text, so the two profilers read either. Needs perf and permission to profile (root, or
  `kernel.perf_event_paranoid <= 1`).
- **`env.sh`** -- sourced by the shell harnesses: `PY` is `$PYTHON`, else `/usr/local/bin/python3`, else `python3` on PATH;
  the open-files soft limit is raised to `$HARNESS_NOFILE` (default 65536, up to the hard limit), since a node started with
  `--tls-max-connections 1024` refuses to start under the usual 1024.
- **`engine_mixed_load.py ... --term-at SECS`** -- T7: SIGTERM that many seconds in under full load; the node must exit 0
  within `--term-bound`, then restarts on the same data and the dump must hold every acknowledged write.
- **`engine_crash_check.sh`** needs the `strata_check` tool: `cmake --build build --target strata_check`
  (found next to the binary, or `CHECK_TOOL=`).
- **`baseline_7c.sh`, `baseline_report.py`** -- the pre-7c rows (T4, T5, T8, T11, T12) in one hold, and their tables.
- **`gate7c_rows.sh REL ASAN OUT`** -- the 7c gate's functional rows (T1 census and `--strict --owed`, T2 on ASan, T3
  `--reactors 2`, T7 on ASan, T8's overloads, T10, X6's shutdown) on a Release and an ASan binary, each row to its end;
  `OUT/rows.tsv` holds each row's exit code. It runs no timing row.
- **`test_gate_tools.py`** -- unit tests for `async_check` and `stall_load` (no node; `python3 -I harness/test_gate_tools.py`).
- **`ruff.toml`, `pyrefly.toml`** -- the lint and strict type configuration for these scripts.
