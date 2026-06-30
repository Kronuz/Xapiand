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

```sh
# correctness (per node, fresh data dir, fixed name)
python3 docs_to_postman.py | newman run /dev/stdin --reporters json --reporter-json-export base.json   # vs baseline build
python3 docs_to_postman.py | newman run /dev/stdin --reporters json --reporter-json-export ours.json   # vs our build
python3 harness/e2e_diff.py  base.json ours.json
python3 harness/bodydiff.py  base.json ours.json

# performance (one node up at a time on :8880)
python3 harness/loadtest.py --datadir <dir> --label baseline --out harness/results/baseline.json
python3 harness/loadtest.py --datadir <dir> --label ours     --out harness/results/ours.json
python3 harness/perfdiff.py harness/results/baseline.json harness/results/ours.json
```

## Baseline + current status (2026-06-30)

Baseline build = `7bd295b` (earliest commit that builds on Apple Silicon; pristine
`origin/master` does not — its toolchain modernization is entangled with
de-vendoring). After the rapidjson fix, the de-vendored build is **green vs
baseline**: 0 assertion regressions, 0 status diffs, and body divergences only in
volatile fields. **Performance is at parity** (query QPS/latency and index
throughput within noise; per-index on-disk identical).

## Known refinements

- The accounts dataset is small (docs go *inplace*), so the **zstd storage codec
  isn't exercised** — add a large-document dataset to benchmark compression.
- The exhaustive differential corpus (type×op matrix, error paths, content
  negotiation, custom verbs) is not yet generated; today the correctness lane
  reuses the 303-request doc collection. Generating that corpus is the path to
  truly exhaustive coverage (the baseline-as-oracle keeps it cheap).
