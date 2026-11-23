# bulk-load-rss: where a bulk RESTORE's peak RSS goes

> Status: CURRENT (2026-11-21) profiles of master's RESTORE (Xapiand:b2cd24fa0's `src/`, equal to master
> Xapiand:79f86b9ef for this path), the explorer's prototype (Xapiand:b60d09d64), and the change that streams a
> top-level array (quest restore-stream, Xapiand:a131ad2b2) against master Xapiand:e1be368395a1.

Question: `docs/perf/bulk-load-rerun.md` (branch `quest/xapiand/bulk-load-rerun`) measured Xapiand peaking at
1,303 MB RSS restoring Kibana's logs sample (14,005 documents, 45.6 MB index) and 1,827 MB restoring shakespeare
(111,396 documents, 19.4 MB index), from 9 MB idle. What holds that memory, and what single change cuts most of it?

## Method

- Binary: the rerun's Release build (Xapiand:29a2aadd7, master b2cd24fa0 plus the harness; LTO, system malloc),
  frozen at `~/.scratch/xapiand/explore-bulk-rss/xapiand-b2cd24f`. Host: macOS 15.8.1, Intel Core i9-9980HK, 32 GiB.
- Load: `harness/bench/bulk_rss_probe.py` (this branch). As the rerun's `xapiand` arm: a fresh server per trial
  (`--shards 1`), one `RESTORE /<name>/?commit` of `es-<name>.msgpack` (logs 47.0 MiB, shakespeare 15.6 MiB), sent in
  1 MiB pieces. It samples `ps` RSS every 20 ms against the bytes sent, and checks `:count` afterwards.
- `--body stream` strips the dump's top-level array header (3 bytes), so the same objects arrive as a stream of
  MessagePack objects, which RESTORE already accepts.
- Attribution: the server under `MallocStackLogging=lite`, stopped (SIGSTOP) once its RSS crosses a threshold near
  the peak, then `malloc_history -callTree` (inverted and not), `heap -s` and `vmmap --summary`. These trials' times
  are not timings.

## Where the memory is

Every `es-*.msgpack` dump is one top-level MessagePack array. RSS climbs steadily from the first prepare to the
closing commit, and peaks just before the response:

| Dataset, body | Peak RSS | Peak at | Last byte accepted | Response | Job |
|---|---|---|---|---|---|
| logs, array | 1,329 MB | 4.82 s | 0.15 s | 4.90 s | 261121-013217-d93ce8 |
| logs, stream | 341 MB | 4.65 s | 3.89 s | 4.64 s | 261121-013218-7c445e |
| shakespeare, array | 1,824 MB | 5.27 s | 0.07 s | 5.42 s | 261121-013219-203ab9 |
| shakespeare, stream | 400 MB | 4.63 s | 3.68 s | 4.70 s | 261121-013220-3ed25e |

(Medians of three trials; the times are trial 1's.) With the array body the server takes the whole body in under
0.15 s: the stream's flow control never pushes back, because nothing is parsed until the array's last byte arrives.

Live malloc bytes near the peak, by allocating site (`malloc_history`, jobs 261121-013221-bcb3be,
261121-013223-854e98, 261121-013606-c8f8c3, 261121-013607-5f0a58):

| Holder | logs, array (RSS 1,100 MB) | shakespeare, array (RSS 1,500 MB) | logs, stream (RSS 300 MB) | shakespeare, stream (RSS 350 MB) |
|---|---|---|---|---|
| Lazy map indexes of the documents' MsgPack trees (`MsgPack::_init_map`, from `Schema::index`, `document_id_of`, the result's `find`) | 527 MB (2.88 M blocks) | 580 MB (3.17 M blocks) | under 3 MB | under 3 MB |
| One msgpack zone per prepared document's data object (`make_shared<zone>` in the prepare's tuple, 8.3 KB each) | 116 MB (14,005) | 925 MB (111,396) | under 3 MB | 4 MB (447) |
| The decoded body: msgpack objects copied into zones (`object_with_zone`), `_init_array` | 172 MB | 89 MB | under 3 MB | 7 MB (one chunk's objects) |
| The run's `Admitted` entries and prepared tuples | 4 MB | 62 MB | under 3 MB | under 3 MB |
| strata's write buffer (prospect tuple buffers, stored bodies, values) | about 150 MB | about 120 MB | about 230 MB | about 160 MB |
| Total live | 996 MB | 1,820 MB | 241 MB | 187 MB |

## Cause

The dump is one MessagePack object: the whole array. `Request::append` (`src/server/search_views.cc`) feeds each body
chunk to msgpack's unpacker, which yields the array only when its last element is complete, so the RESTORE's one
batch is the whole dump. `DocIndexer::index` (`src/database/handler.cc`) then takes the array's elements into one run
and frees the run only after all of it is applied (`run.clear()` at the end of `apply_run`). Until then every
document keeps its decoded tree, the map indexes its prepare built over it, and its prepared data object with a zone
of its own. The cost per document is per node and per zone, not per byte, which is why shakespeare's small documents
cost 16 KB each and logs' large ones 93 KB.

The suspects, against the evidence:

1. The whole request body buffered: yes, as a side effect (the unpacker holds the 47 MiB), but it is 4% of the peak.
2. Decoded documents and per-document buffers kept until the commit: yes, the cause. They are kept until the run
   ends, and the run is the whole dump.
3. strata's write buffer before the commit: 120-230 MB, the largest holder once the body streams.
4. Allocator retention: not in the peak (the live malloc total accounts for it). After the response `ps` RSS stays
   at 1.2-1.4 GB, but `vmmap` shows the freed pages are clean and reusable: the physical footprint is 91-111 MB
   (`*-after-vmmap.txt` of job 261121-013217-d93ce8 and 261121-013219-203ab9). `ps` RSS overstates a process
   after a large free on macOS; physical footprint is the figure macOS charges.

## The change

`Request::append` (`src/server/search_views.cc`) takes a non-empty top-level array's header (1, 3 or 5 bytes, however
the body's chunks cut it) and parses the elements as they arrive, with an unpacker one level shallower than the
body's, the array being one of its `MAX_JSON_DEPTH` levels. Each chunk's complete elements reach the indexer as one
array, the shape the whole array had, so `DocIndexer::index` expands them exactly as before: a nested array element
stays one element (refused as a non-object), and no element's `_settings` becomes the index's. Batches are one
chunk's elements, applied and freed before the next chunk is parsed, and the stream's flow control paces the client.
An empty array is not streamed: the unpacker reads its header and yields `[]` as before.

The header's count holds. A body that ends short of it, or carries anything past it, answers 400 (`The request body
ended inside its top-level array`, `The request body holds more than its top-level array declares`), and an element
the unpacker refuses (malformed, or nested deeper than the whole array allowed) answers 400 naming it. Master
answered these 200, restoring nothing (a short array, a malformed or too-deep element) or the surplus as more
documents (D162). Elements of chunks before the failing one are restored, as NDJSON's lines before a bad one; the
failing chunk's elements and the rest are not, where NDJSON skips the bad line and goes on.
`tests/test_restore_stream.cc` holds each body's answers, master's captured first.

Measured, the array body, five interleaved rounds of master, the change and master again (A/A), one trial each,
`strict` (Release, system malloc; each job ran beside two light holds), with the probe at Xapiand:5842f9ef3:

| Dataset | Peak RSS master | Peak RSS change | Peak RSS A/A | Seconds master (min-max) | Seconds change (min-max) | Seconds A/A (min-max) | Job |
|---|---|---|---|---|---|---|---|
| logs | 1,357 MB | 344 MB (75% less) | 1,351 MB | 8.47 (7.72-10.06) | 9.19 (8.55-11.60) | 7.98 (7.84-11.27) | 261121-034215-99dd61 |
| shakespeare | 1,860 MB | 405 MB (78% less) | 1,854 MB | 11.21 (9.49-11.44) | 7.65 (7.28-9.82) | 9.66 (8.70-11.62) | 261121-034217-cc2491 |

(Medians; the peaks' spread is 1,326-1,378, 337-350, 1,333-1,360 MB on logs and 1,848-1,864, 376-411, 1,848-1,864 MB
on shakespeare, so the memory cut is far outside it.) The first pair of jobs, run with the probe at Xapiand:a131ad2b2
(261121-020723-9399ae, 261121-020724-73f7ef), gave 1,322 -> 333 MB on logs and 1,846 -> 423 MB on shakespeare, within
4% of these. The explorer's prototype, which only skipped the header and expanded a nested array element, peaked at
339 and 396 MB (E3).

The times are not a result either way. On shakespeare the change's median is below master's and the A/A's in both
pairs of jobs (7.65 s against 11.21 and 9.66; 10.21 against 13.43 and 11.25). On logs the change's median is above
master's in this pair (9.19 s against 8.47, 8% more, and 7.98 for the A/A) and below it in the first (9.59 against
9.79); a job's master and its A/A differ by up to 16% (11.21 against 9.66 on shakespeare) and a round's time by up to 40%, so a gap of
that size is inside the spread but is not shown to be absent. The times are about twice the explorer's (4.9 and 5.6 s)
on a busier host. An exclusive run on logs settles the direction there (261121-040146-c8fed8, five interleaved rounds at Xapiand:5842f9ef3): master 5.05 s (4.91-6.33), the change 5.16 s (4.98-5.19), the A/A 4.96 s (4.80-5.02), so the change is 2% slower, inside the 2% between master and its A/A: no measurable time cost, with peak RSS 1,311 MB against 336 MB (74% smaller). `bulk_rss_probe.py --identity` checks that restoring each dump as its array and as the same objects
streamed without it leaves the same documents (261121-034214-a0dea9: shakespeare's ids and documents exactly; logs
carries no `_id`, so its documents are compared without the ids each restore generates).

## What remains

With the body streamed, the peak is 344-405 MB, held mostly by strata's write buffer until the RESTORE's one closing
commit (about 230 MB live on logs and 160 MB on shakespeare in the stream-body attribution above; a RESTORE opens its
shard with `DB_DISABLE_AUTOCOMMIT`). The rest is one chunk's decoded elements and their prepared state, and the
process baseline. The next lever is that buffer's flush share during a long restore. Shakespeare's 925 MB of
per-document zones also says a MsgPack's 8 KB zone per small object is costly wherever many objects are held at
once; streaming removes the case here.

## Jobs

| Job | What |
|---|---|
| 261121-013217-d93ce8, 261121-013218-7c445e, 261121-013219-203ab9, 261121-013220-3ed25e | E1: peak RSS and time at master, array and stream bodies |
| 261121-013221-bcb3be, 261121-013223-854e98, 261121-013606-c8f8c3, 261121-013607-5f0a58 | E2: `malloc_history` near the peak, array and stream bodies |
| 261121-013602-c1cadb | Release build of the prototype |
| 261121-013604-b2c5a2, 261121-013605-c47571 | E3: peak RSS and time with the prototype, array bodies |
| 261121-034215-99dd61, 261121-034217-cc2491 | The change: peak RSS and time, master, change and A/A interleaved (probe at Xapiand:5842f9ef3) |
| 261121-020723-9399ae, 261121-020724-73f7ef | The same, first pair (probe at Xapiand:a131ad2b2) |
| 261121-034214-a0dea9 | The change: identity of the array and stream restores' DUMPs |

Captures (call trees, timelines, vmmap) are under `~/.scratch/xapiand/explore-bulk-rss/{e1,e2,e3}`.
