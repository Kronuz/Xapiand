# Bulk index profile: where a RESTORE spends its time

> Status: CURRENT (2026-10-27) explore/bulk-profile from Xapiand:6c5c1a40d (Xapian engine); local only, not pushed. Measurement, no product change.

Question: where does the time of a 20,000-document bank RESTORE go, by layer, and which of those layers are worth optimizing given that Xapian, the WAL, the offload bridges and the remote/replication protocols are being replaced?

## Method

- Binary: `RelWithDebInfo -O2 -g`, `-fno-omit-frame-pointer`, LTO off (so layers stay visible). x86_64 macOS (i9-9980HK, 8 physical cores), single `--solo` node.
- Load: `docs/public/assets/accounts.ndjson` x 20 = 20,000 docs, one `RESTORE /bench/?commit` (the developer's seam-write corpus and client).
- Harness: `harness/profile_bulk.sh BIN OUTDIR` (build-agnostic, rerunnable): a timed smoke run (docs/s, server user/sys from `wait4`), then a fresh node sampled end to end with `sample(1)`; samples are classified by the deepest frame that matches a layer rule (leaf to root) and by thread group; JSON and markdown are written beside the raw sample. Rules live in `LAYERS` in the script. `--schema JSON` PUTs a schema first, `--reanalyze` redoes the analysis from a kept sample.
- Held with `quiet strict`. One smoke and one profile per variant (no repeats). Spread of the baseline docs/s over three separate runs: 4,240 / 4,510 (harness) / 4,635 (lead's number), about 5%. Percentages are shares of busy samples, not exact CPU.
- Raw samples, JSON and logs: `~/code/Kronuz/Xapiand/.scratch/bulk-profile/` (`base.*`, `nodelwal.*`, `noindex.*`; `s1.txt` is an 8 s steady-state sample of a 100k-doc run).

## Baseline result

4,510 docs/s, wall 4.43 s, user 17.1 s + sys 4.9 s = 1.10 ms CPU per document.

| layer | status | % of CPU | % of critical thread | top 3 functions |
|---|---|---:|---:|---|
| Xapian engine (add/replace/commit) | replaced (S1) | 27.0 | 31.3 | `Inverter::set_positionlist`, `io_read_block` (pread of glass blocks), `CompressionStream::compress` (zlib of termlists) |
| Xapian::Document in-memory build | replaced (S1) | 10.1 | 4.5 | `Document::Internal::add_posting`, `add_term`, `~Internal` |
| WAL delete/write | replaced (7b) | 20.4 | 0.9 | `delete_files` (directory scan), `wait_dequeue_timed`, `DatabaseWALWriter::enqueue` |
| Shard checkout / sync_wait / lock | replaced (7c) | 9.0 | 12.2 | `ShardEndpoint::checkout`, `_writable_checkout`, `checkin` |
| Wait on the shard writable checkout | replaced (7c) | blocked | 41.2 | condvar in `_writable_checkout` |
| Data record (zstd, locator, `Data::set_obj/get_obj`) | stays | 9.2 | 7.6 | `ZSTD_compressBlock_doubleFast`, `finalise_write`, `HUF_buildCTable_wksp` |
| Schema processing | stays | 8.7 | 0 | `Schema::index_subproperties`, `index_new_object`, `get_partial_paths` |
| Doc pipeline glue (prepare/index, queues) | stays | 7.7 | 2.0 | `wait_dequeue_timed` (pool polling), `DocIndexer::operator()`, `DocPreparer` |
| Term generation (analysis, snowball) | stays | 6.5 | 0 | `Schema::index_item`, `term_generator::emit`, `term_generator::word` |
| JSON/MsgPack decode | stays | 1.0 | 0.3 | `msgpack object_with_zone`, `rapidjson ParseString` |
| HTTP/reactor/request parsing | stays | 0.1 | 0 | `Request::append`, `non_blocking_recv1` |
| other (malloc, runtime) | | 0.2 | 0 | |

"Critical thread" is the busiest shard-writer thread (`DI`): its busy plus blocked timeline, the part that serializes docs/s. Columns do not add to 100% for the critical thread because DocPreparer work (schema, term generation) runs on separate `DP` threads.

Thread picture: `DI` (shard writers) are 10.2k busy and 6.9k blocked samples; `DP` (prepare: schema + terms) 5.5k busy across 10 threads; `WL` (WAL writer) 4.0k busy; the HTTP/parse thread is about 7% busy (about 200 samples).

## What bounds throughput

The per-shard write path bounds docs/s: each `DI` thread spends about 41% of its timeline blocked on the shard's writable checkout (another `DI` thread holds it), 31% in the Xapian replace, 12% in checkout/checkin bookkeeping, 8% in the data record and glue. HTTP, decode, schema and term generation (the `DP` side) are parallel and never the limit; together HTTP + decode are about 1% of CPU (a one-thread ceiling around 80k docs/s).

Amdahl, CPU-bound on 8 physical cores (`cores x 1000 / remaining CPU ms per doc`):

- Xapian rows (engine + Xapian::Document) to zero: 0.79 ms CPU/doc left, about 11,600 docs/s (2.6x faster than 4,510).
- Every replaced layer to zero (also WAL and checkout): 0.368 ms CPU/doc left, about 21,800 docs/s (4.8x faster), if the surviving stages scale across the cores.

## Cross-checks

(b) WAL. RESTORE already opens shards with `DB_DISABLE_WAL`, but `ShardEndpoint::checkin` (`src/database/pool.cc`) still enqueues `wal_writer->delete_wal` for every restore checkin, so the WAL writer scans and deletes `wal.*` per document: 20% of CPU on the `WL` threads (`delete_files`: getdirentries/open/fstatfs). A build that skips that call (`.scratch/bulk-profile/nodelwal.patch`; `DATABASE_WAL=OFF` does not compile at this commit: incomplete `DatabaseWALWriter` in a unique_ptr) gives 4,495 docs/s versus 4,510, server sys time 4.9 s to 2.2 s, user unchanged. So the WAL costs about 0.14 ms CPU/doc but is not on the critical path today; it matters once the engine is fast and CPU is the limit. Replaced by 7b, not a target; the replacement must beat about 0.14 ms CPU per document.

(a) Index off. Root schema `{"_schema":{"_index":"none"}}` (no terms or values for any field): 12,215 docs/s (2.7x faster), 0.57 ms CPU/doc (from 1.10), Xapian rows fall from 37% to 6% of CPU, Xapian::Document build to 0.8%. Schema processing stays at 16% of what is left (about 0.094 ms/doc, unchanged from baseline's 0.096), glue grows to 29%, the data record to 11%. So term and value indexing through Xapian is about half of today's CPU, and schema walk cost is a fixed per-document tax.

## What the replaced layers cost today (what replacements must beat)

Xapian 37.1% (0.41 ms CPU/doc; the shard replace path is 36% of the critical thread), WAL 20.4% (0.22 ms/doc by samples; 0.14 ms/doc by rusage sys time, off the critical path), checkout/sync_wait 9.0% plus 41% blocked wait on the critical thread (0.10 ms/doc CPU; the lock wait alone is why 5 shard threads reach only about 50% busy). Remote and replication protocols are not on this path (single node).

## Targets among the layers that stay, by expected docs/s gain

Gain is per-document CPU saved over the 0.368 ms/doc that remains once the replaced layers are gone (about 21,800 docs/s ceiling), assuming the stated saving.

1. Data record (9.2% of CPU, 0.10 ms/doc): `compress_zstd` builds a fresh dictionary context per document (`ZstdCompressData::init` -> `ZSTD_compress_usingDict` -> `HUF_buildCTable_wksp`), and `finalise_write` decodes the data back (`Data::get_obj`) just after encoding it. A reused `ZSTD_CDict`/context and no decode roundtrip should save about 60% (0.06 ms/doc): +20% docs/s (about 21.8k to 26k).
2. Schema processing (8.7% of CPU, 0.096 ms/doc): every document re-walks the schema (`index_subproperties`, `get_partial_paths`, `feed_subproperties`, MsgPack map/body churn). Caching the resolved per-field specification for repeated shapes should halve it: +15% (about 25k). Largest after the data record, and it is the per-doc tax that survives with indexing off.
3. Pipeline glue and queue handoffs (7.7%, 0.085 ms/doc): DocPreparer -> DocIndexer queue wake-ups, `wait_dequeue_timed` polling in the pool and scheduler threads, MsgPack destruction. Batch handoffs and untimed waits: about +10% .
4. Term generation (6.5%, 0.07 ms/doc, after the Xapian::Document build moves to strata's builder): allocation-heavy `emit`/`word` path; about +5 to 8% with arena or reused buffers.

JSON/MsgPack decode and HTTP are about 1% of CPU: no target.

## Caveats

Samples use a 1 ms nominal interval that stretches to about 1.7 ms under load; shares are reliable, absolute core counts are not. `sample` counts a thread blocked in `pread` or a mutex wait as busy (mutex waits are broken out only for the shard checkout). Single runs per variant; the `nodelwal` profile has fewer wall samples than the baseline (a shorter sampled window), so use its docs/s and CPU, not its layer split.
