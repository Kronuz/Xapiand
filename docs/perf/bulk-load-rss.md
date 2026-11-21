# bulk-load-rss: where a bulk RESTORE's peak RSS goes

> Status: CURRENT (2026-11-21) profiles of master's RESTORE (Xapiand:b2cd24fa0's `src/`, equal to master
> Xapiand:79f86b9ef for this path) and a prototype fix on branch `explore/bulk-load-rss` (Xapiand:b60d09d64).

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

Stream a top-level array's elements: `Request::append` skips the array's header (1, 3 or 5 bytes) at the start of a
streamed MessagePack body, so the unpacker yields each element as it completes, as for a stream of objects. Batches
are then one body chunk's objects, applied and freed before the next chunk is parsed, and the stream's flow control
paces the client. Eleven lines on this branch (Xapiand:b60d09d64).

Measured, the array body before and after (three trials each, strict):

| Dataset | Peak RSS before | Peak RSS after | Seconds before (min-max) | Seconds after (min-max) | Jobs before, after |
|---|---|---|---|---|---|
| logs | 1,329 MB | 339 MB (74% less) | 4.894 (4.86-5.23) | 4.953 (4.66-5.20) | 261121-013217-d93ce8, 261121-013604-b2c5a2 |
| shakespeare | 1,824 MB | 396 MB (78% less) | 5.577 (5.42-5.70) | 5.731 (5.54-5.77) | 261121-013219-203ab9, 261121-013605-c47571 |

Each trial's `:count` matched (14,005 and 111,396). The peak lands on the stream body's (341 and 400 MB at master),
as it should: the server now takes the body at the pace it indexes (last byte at 3.9-4.5 s instead of 0.07-0.15 s).
Load time does not move beyond noise (1% and 3% slower, ranges overlapping). One figure is unexplained: master's
own stream body loaded shakespeare in 4.789 s (4.69-4.80), 14% faster than either array run; the prototype's array
body does not reproduce it. It is a different build of the same flags run in a different job, so host state is the
likeliest cause; a fix quest's timing would settle it with both bodies on one binary in one exclusive job.

One behavior differs: an element of the top-level array that is itself an array is now expanded into its objects (a
batch's array is "the objects it holds"); before, it was rejected as a non-object. A fix quest would keep the old
rule or document the new one, and add a test for a dump that splits an array's header across body chunks.

## What remains

With the body streamed, the peak is 341-400 MB, of which strata's write buffer is the largest holder (about 230 MB
live on logs and 160 MB on shakespeare, growing with the restore until the closing commit; a RESTORE opens its shard with
`DB_DISABLE_AUTOCOMMIT`). The next lever is that buffer's flush share during a long restore. Shakespeare's 925 MB of
per-document zones also says a MsgPack's 8 KB zone per small object is costly wherever many objects are held at
once; streaming removes the case here.

## Jobs

| Job | What |
|---|---|
| 261121-013217-d93ce8, 261121-013218-7c445e, 261121-013219-203ab9, 261121-013220-3ed25e | E1: peak RSS and time at master, array and stream bodies |
| 261121-013221-bcb3be, 261121-013223-854e98, 261121-013606-c8f8c3, 261121-013607-5f0a58 | E2: `malloc_history` near the peak, array and stream bodies |
| 261121-013602-c1cadb | Release build of the prototype |
| 261121-013604-b2c5a2, 261121-013605-c47571 | E3: peak RSS and time with the prototype, array bodies |

Captures (call trees, timelines, vmmap) are under `~/.scratch/xapiand/explore-bulk-rss/{e1,e2,e3}`.
