> Status: CURRENT (2026-11-04) how the closing benchmark's harness works, from the code on `fix/closing-bench-harness` (workstream `closing-bench-harness`). Design: `docs/design/closing-benchmark.md`, revision 1.

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

### The marked write stream (T7, C5)

`closing_mixed.py --stream-corpus` is T7 as the design runs it. The index already holds the base (`--keep`: the data in
OUTDIR/data is a pristine copy, started without wiping; or `--base-corpus` loaded first). `--writers` threads append
batches of the C-logs write stream at `--write-rate` documents a second; each document gains `stream` (`t7w<writer>`) and
`seq` (1, 2, ...), so its identity is `t7w<writer>-<seq>`. Every id is recorded before the request and its
acknowledgement time after; the full list goes to `--sent-log`. At `--kill-at` the system gets SIGKILL and restarts on the
same data; a `commit=force` poke makes Quickwit publish what its log replayed. The dump (`dump_stream`: Xapiand a sorted
search paged by `_offset`, Elasticsearch a scroll, Quickwit a sorted search paged by `start_offset`) covers only the
marked streams. `audit` then reports lost beyond the window (D5: 2 s for S and X, 5 s for E and Q), late inside it (count
and oldest age), present but never sent, duplicated; the base count (all documents less the marked and the poke) must
equal the count before the writes. The old `mix` mode (puts, bulks, replaces, deletes on its own index) is kept as a
correctness run.

## Corpora, query sets and the lease

- `closing/fetch_corpus.sh logs|logs-writes|wiki|wiki-queries [--limit N]` downloads on the machine that uses the data,
  checks each download against `closing/corpora.pins` (sha256, bytes) and each file's line count, and writes under
  `$CLOSING_DATA/corpora` (default `~/data`, outside the `~/work` that `lease run --sync` replaces). A sample goes to
  `corpora/sample-N/`. C-wiki passes through `closing/wiki_transform.py` (search-benchmark-game's `corpus_transform.py`).
- `closing_queries.py wiki|logs` writes the neutral query sets: C-wiki's 40 per class from `queries.txt` (the term class
  from the vocabulary of its queries) and C-logs' templates from the corpus's time range, sizes and statuses.
- `closing/setup.sh` is the idempotent `lease run --setup`: the CI packages, the pinned Elasticsearch and Quickwit
  tarballs, `ab_compare.py` at its pinned commit, and a Release gcc-13 build of Xapiand, all under `~/data`.
  `closing/live_proof.sh` is the lease command that runs the tests and a short run of each tool against the three systems.

## closing_bench.sh, closing_units.py, closing_report.py

```
closing_bench.sh --units LIST --deadline-min N      host identity + fio, then closing_units.py run
closing_units.py   units (D14's table) -> pack() -> runs; run: arms in rotated order -> ROOT/UNIT/ARM/rN.json
closing_rows.py    tool JSON -> ROOT/UNIT/ARM/rN.rows.json   (ranked metrics, direction, axis)
closing_report.py  rows -> anchors (rounds 1 and 2) -> ab_compare S vs finalist, S' as A/A -> D11 verdicts
```

- A **unit** is one row's round (`t4b-r3`) or a build (`B1`, `B2`). `pack()` is first-fit decreasing inside each phase
  (A setup, B builds, C calibration, D confirm, E reports); a unit longer than the run's budget (45 minutes less resume,
  sync, setup, archive and margin) is refused. `run` skips a unit whose estimate does not fit before `--deadline-min` and
  lists it in `skipped.tsv`: an overrun would lose the whole run's `$OUT`.
- An **arm** runs from a stopped process after `sync` and a page-cache drop, under `systemd-run --scope` with a cpuset and
  a memory limit, its tool pinned to the generator cores with `taskset`. Rounds 1 and 2 run every arm; later rounds run S,
  S' (S's binary again), X and the arms `closing_report.arms_for_round` keeps (each metric's anchor and the arms within
  10% of it). Each round starts one arm later.
- **Rows.** T1 and T2 load fresh and give documents a second, CPU-seconds per 1,000 documents, peak memory and (T3) bytes
  a document; T4 starts on the built index (`loadtest.py --keep`), runs the latency pass and the clean restart (T7's
  clean row, from the same run); T5 and T6 run `stall_load.py` on the built index or on a pristine copy; T7 crash is the
  stream mode on a pristine copy. B1 and B2 build the full indexes of the four systems together in disjoint cpusets.
- **Verdicts.** `closing_report.py report ROOT` runs `ab_compare.py compare --a S --b FINALIST --aa Sp --output csv` per
  finalist (the anchor and the arms near it that ran five rounds) and once against X. A row is better when the verdict is
  `slower` (S ahead beyond the A/A band), `high` is below 1 and the median ratio at most 0.97; by the margin when `high`
  also meets the axis's ratio (0.7 query, 1/1.3 indexing, 0.7 memory, 0.8 disk); worse when mirrored. `ab_compare` drops
  a pair whose value is not positive, so a verdict needs `pairs` = 5 (C6); anything else is "not distinguishable". A row
  worse than X is flagged REGRESSION.
