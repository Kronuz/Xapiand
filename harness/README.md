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

## Cluster lane

The correctness + performance lanes both run a single **`--solo`** node, so
**Discovery, the Remote protocol, and Replication are never exercised** by them.
`cluster_check.sh` is the missing net for that data plane — the prerequisite
before migrating those services off libev (onto the Asio runtime, Kronuz/reactor).

- **`cluster_check.sh [N]`** — spins an N-node (default 2) localhost cluster and
  asserts the three things `--solo` can't:
  1. **Discovery** — every node converges on the full membership and one leader is
     elected (`GET /` `nodes[]`).
  2. **Replication / Remote** — a doc written on node1 becomes readable from every
     other node (shards are distributed across nodes and reached over the Remote
     protocol / replicated; the read is eventually-consistent so it polls).
  3. **Distributed search** — a `SEARCH` on the last node gathers hits across all
     nodes' shards.

  Unlike the correctness lane this is **not** baseline-differential — it's a
  self-checking GREEN/RED liveness assertion of the cluster data plane (exit 0 =
  all checks passed). Nodes share one discovery port (multicast, `IP_MULTICAST_LOOP`)
  but each takes its own http/xapian/replica TCP ports; cluster settle and the
  first cross-node write are slow (first-touch shard creation), so timeouts are
  deliberately generous. Needs a multicast-capable interface up.

  ```sh
  harness/cluster_check.sh        # 2 nodes
  harness/cluster_check.sh 3      # 3 nodes
  ```

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

## Baseline + current status (2026-07-05)

The e2e baseline was originally build `7bd295b` (earliest commit that builds on Apple
Silicon; pristine `origin/master` does not — its toolchain modernization is entangled
with de-vendoring), and the de-vendored + Xapian 2.0.0 migration was validated **green
vs that oracle**: 0 assertion regressions, 0 status diffs, body divergences only in
volatile fields, performance at parity.

That migration has landed, so the baseline is now **refreshed to this project's own
build at `23e1e4540`** (post native multi-db distributed match). `e2e_check.sh` diffs a
fresh capture against it: assertion parity is GREEN, and the only body divergences are
the genuinely run-to-run volatile fields (the POST auto-id and the `/:metrics`
counters). The check is now a forward-looking regression net for the current
architecture rather than a migration oracle.

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
- **`profile_light.sh`** -- CPU per light request by group (offload hop, scheduler, engine, HTTP)
  from `sample`, and the hop's wall time from an http `trace/hop` build (`-DHTTP_TRACE_HOP`);
  `--compare PRE POST` is T11.
- **`baseline_7c.sh`, `baseline_report.py`** -- the pre-7c rows (T4, T5, T8, T11, T12) in one hold, and their tables.
- **`test_gate_tools.py`** -- unit tests for `async_check` and `stall_load` (no node; `python3 -I harness/test_gate_tools.py`).
- **`ruff.toml`, `pyrefly.toml`** -- the lint and strict type configuration for these scripts.
