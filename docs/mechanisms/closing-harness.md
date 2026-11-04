> Status: CURRENT (2026-11-04) how the closing benchmark's harness works, from the code on `fix/closing-bench-harness` (workstream `closing-bench-harness`). Design: `docs/design/closing-benchmark.md`.

# The closing benchmark's harness

One adapter module gives the existing Xapiand load tools a `--system`: Xapiand (S and X, the same adapter), Elasticsearch
(E) and Quickwit (Q). The tools build requests through the adapter and run them with their own HTTP clients.

```
corpora (ndjson)   neutral queries (jsonl)   harness/closing/schemas.json
        |                  |                          |
        +------------> systems.py: System adapter <---+
                         start/stop  create  bulk  commit  search/count  effective  cache_counters  disk  cgroup
        loadtest.py   stall_load.py   closing_tenants.py   closing_mixed.py     (callers; each owns its client)
        T1-T4, T7     T5, T6          T8-T10               T7 crash audit
```

## systems.py

- **Neutral schema** (`closing/schemas.json`): field name to `text`, `keyword`, `integer` or `date`, an optional id
  field (sent as the document id: Xapiand `_id`, Elasticsearch `_id`; not indexed) and an optional time field
  (Quickwit's `timestamp_field`). Each adapter turns it into its own mapping: Xapiand `_schema`, Elasticsearch
  `mappings` (`dynamic: false`, dates `epoch_second`), Quickwit `doc_mapping` (`record: position` on text, fast fields on
  numbers, dates and keywords, `lenient` mode).
- **Neutral query** (one JSON line, `Query.parse`): `class`, `field`, `terms` with `mode` (`all`, `any`, `phrase`),
  an optional `range`, `sort`, `agg` (`date_histogram`, `terms`, `stats`), `filter` (one exact term, a tenant) and
  `command`. `vary(run, seed)` shifts a range's bounds by 0 to 9 units from a stream seeded by seed, run and query
  (run 0 is the query as written), so repeated runs do not repeat a filter.
- **Commands and count modes:** `search` is a top-10 whose total may be a lower bound (E `track_total_hits: false`, Q
  `count_all: false`, X without `check_at_least`); `search_exact` and `count` are exact on every system (E
  `track_total_hits: true` or `_count`, Q the default, X `check_at_least` at 4,294,967,295). D8's check runs in
  `search_exact`.
- **Result caches off:** E has `index.requests.cache.enable: false` in its index settings and `request_cache=false` on
  every search; Q's node config has `partial_request_cache_capacity: 0`; Xapiand has no result cache (grep of `src/`).
  The filter caches (E's node query cache, Q's predicate cache) stay on and are reported. `cache_counters` reads E's
  `_nodes/stats/indices/request_cache,query_cache` and Q's `/metrics` `cache_hits_total{component_name=...}`.
- **Probe** (`systems.py probe`): the result-cache hit counters over a T4-style pass (every query ten times as search
  and as count, bounds varied) stay at 0; E's request cache is shown alive by asking for it per request; Q's by a second
  launch with `Launch.caches_on`; a range template with one bound and with ten draws (filter-cache hits for each); a term
  over 10,000 documents in the top-10 and count modes; the effective settings.
- **Check** (`systems.py check`, `check_systems`): per query in `search_exact`, counts compared across systems (a gap
  over 1% drops the query from all, and more than 5% of a class stops the run), S against X compared on count and top-10
  order, aggregation buckets compared bucket by bucket after normalizing keys (date buckets to epoch seconds).
- **Lifecycle:** `start` writes the system's config (`closing/xapiand.opts`, `elasticsearch.yml`, `quickwit.yaml`, index
  templates) into the work directory, launches under `Launch.prefix` (for example a `systemd-run` scope) in a new
  session, and waits for readiness; `stop(kill=True)` is SIGKILL to the group; `spawn`/`restart` start again on the same
  data. `effective` reads the settings back (Xapiand `GET /`, E `_nodes` and `_settings`, Q `/api/v1/version`, `/config`);
  `assert_effective` lists the differences from what D2 asked. `disk_bytes` is `du`'s measure (allocated blocks) once
  stable for 1 s, after `durable` (E `_flush`, then `sync`). `Cgroup` reads `memory.current`, `memory.stat`, `cpu.stat`.

## loadtest.py

Legacy mode (accounts, Xapiand, one RESTORE, closed-loop query mix) is unchanged. Closing mode (`--corpus --schema`):
`load_index` runs `--clients` senders over batches of `--batch-docs`, the last batch alone with the commit flag, then
`commit`, a poll until every document is searchable, `durable`, `du`; it records documents per second, CPU-seconds per
1,000 documents (cgroup), peak memory (cgroup or RSS) and bytes per document. `--resume STATE` loads in segments, each
fully acknowledged before the next, and a rerun skips the segments recorded. `--tune` finds the batch size and sender
count by doubling until a step gains under 10%. `--latency` (T4): warm-up by `--warm-secs`, then each query ten times as
search and as count; a range template's statistic is the median of its draws and any other query's the fastest run; per
class the geometric mean, p50 and p99; the client floor (E `GET /`, Q `/api/v1/version`, X a missing id) and the
server's CPU per query beside; the run is void if a result-cache counter moved. `--restart` is T7's clean restart:
stop, drop caches, start, poll a count every 10 ms until it finds documents.

## stall_load.py --system

`--saturate` (T5): for each class, the open-loop rate is doubled from `--saturate-start` and then bisected; a step is good
when p99 is within the class bound (25 ms, 250 ms for aggregations), the achieved rate within 1% of offered, no errors, no
backlog and every generator under 80% CPU. The schedule is split over `--procs` spawned processes: unit i goes to process
i mod N at `t0 + i / rate` on one shared wall-clock `t0`; each draws its range bounds per request. Server CPU per query
comes from the cgroup. `--mixed` (T6): open-loop writes in batches from `--write-corpus`, open-loop searches from the
other processes, one probe document a second, and one poller every `--probe-poll` seconds counting any probe written so
far; probe k is visible when the count first reaches k.

## closing_tenants.py (via `wide_search_check.py tenants`) and closing_mixed.py (via `engine_mixed_load.py --system`)

`closing_tenants`: tenants of P documents cut in order from a corpus; per-index (one index per tenant, creation stopped
at `--budget-s`) or shared (a `tenant` field; Q partitions on it). Readings at a quarter, half and all of `--n-min`
tenants opened, 60 s idle at `--n-min`, descriptors from `/proc`; then restart with the page cache dropped and one query
to each of `--cold` random tenants; then `--runs` searches across every tenant. `closing_mixed`: writers (puts, bulk of
20, replaces, deletes where the system has them) and searchers on one index; every acknowledgement is recorded with its
time; `--kill-at` sends SIGKILL, restarts, pokes, dumps every id through the adapter, and audits against the model: lost
beyond the window (S and X 2 s, E and Q 5 s), lost inside it (count and oldest age), present but never written, duplicated.
