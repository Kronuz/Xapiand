# restore-stall: what a RESTORE holds the reactor loop for, and what the lag probe's 49-82 ms is

> Status: CURRENT (2026-11-16). Exploration on `explore/restore-stall` at master Xapiand:d80a17ee2; the fix of answer 1
> and its A/B are in [Fix: the results in one buffer](#fix-the-results-in-one-buffer). The sliced free
> (`harness/restore_stall_slice_results.patch`) is the measured fallback, not applied.
> Follows `docs/perf/reactor-offload.md` (branch `explore/reactor-offload`), whose two open rows these are.

Answers.

1. A RESTORE chunk ends by freeing its per-object result maps in one turn: `~DocIndexer` at the end of
   `restore_database_view` (`src/server/search_views.cc:1774`, `DocIndexer`'s `std::vector<MsgPack> _results`,
   `src/database/handler.h:361`) holds the loop 10.6-24.7 ms, once per 5,000-document chunk, 81% of it inside
   `madvise` called by Darwin's `free`. It is every turn over 3 ms in the RESTORE phase (237 of 238 stall captures).
   Prepare, `Schema::index` and the apply are not on the loop any more: they are 0% and 1.3% of its busy samples. Freeing
   the results in slices moves the trivial GET's p99 from 22.4-24.1 ms to 5.6-5.8 ms (A/A 2.2 ms).
2. The lag probe's 49-82 ms is the `GET /:metrics` scrape's own turn, not the probe's floor and not a periodic task:
   `XapiandManager::server_metrics_impl` (`src/manager.cc:1315`) polls every descriptor up to `RLIMIT_NOFILE`
   (`system.cc` `get_open_files_per_proc`), 52-56 ms of loop time per scrape at the default limit. A quiet node has no
   loop turn over 3 ms in 60 s.

## Method

Binary: master Xapiand:d80a17ee2, RelWithDebInfo, LTO off, `-fno-omit-frame-pointer`, built under `quiet light`, plus
`harness/restore_stall_reactor_lag.patch` on the pinned reactor's `reactor_lag.h` (env-gated, inert unless
`XAPIAND_LAG_SPIN` is set). Host: macOS 15, Intel i9-9980HK, 16 logical CPUs; every timed arm ran inside one `quiet strict`
hold (exclusive on macOS): `rs-measure` (job at 06:10) and `rs-measure2` (06:49).

Probe: `harness/restore_stall_probe.py` (reuses `reactor_offload_probe.py`'s `phase`). One node, `--processors 1` (one
HTTP loop plus the control loop), 20,000 `accounts`, 20 s phases: a trivial GET at 200/s over 16 connections (first 2 s
dropped, 3,600 samples), then the same with a client RESTOREing 5,000-document chunks back to back into another index
(`?commit`). The A/A is the trivial phases before and after the RESTORE phase and a whole second run (`a1`, `a2`).

Loop probes:

- the node's own `reactor::LagProbe` (`xapiand_reactor_lag_seconds`, 1 ms timer when busy, 100 ms idle);
- `XAPIAND_LAG_SPIN=3000`: a sleep-free probe, a handler on the loop that re-posts itself and logs every gap over 3 ms
  between two of its runs (so, every loop turn over 3 ms), plus a watchdog thread that signals the loop thread every
  1.5 ms of a stall and walks its frame pointers (`STALL` lines, symbolized by `harness/restore_stall_stalls.py`);
- `sample` at 1 ms on the node for the RESTORE phase (`harness/restore_stall_profile.py` reads the loop thread; the
  sampler stops the process, so its run gives shares, not latency).

## 1. RESTORE: which turn holds the loop

A/A and the reproduction, 1 reactor, trivial GET latency in ms (p99 / p99.9 / max; loop lag p99.9):

| Arm | trivial | trivial2 | RESTORE phase | trivial3 |
|---|---|---|---|---|
| run a1 (clean) | 2.16 / 2.48 / 7.1 | 2.15 / 3.00 / 4.4 | 23.6 / 32.8 / 36.2; 20,480 us | 2.24 / 2.47 / 6.6 |
| run a2 (clean) | 2.18 / 2.54 / 7.2 | 2.23 / 2.99 / 4.3 | 24.4 / 34.9 / 44.8; 20,480 us | 2.24 / 2.95 / 7.6 |

The A/A spread is 0.1 ms at p99. RESTORE: 6,724-6,728 docs/s, 135,000 documents in the phase, 27 chunks.

Where the turn is. The spin probe on master (`spin-master`): in the RESTORE phase 27-28 loop turns over 3 ms, 10.6-24.7 ms
each (median about 18 ms), one per chunk; none in the 60 s quiet stretch, none in the trivial phases. The watchdog's 238
stack captures in that phase: 237 are

```
madvise (libsystem_kernel)  <- free_small / mvm_madvise_free (libsystem_malloc)
  <- std::__shared_ptr_emplace<msgpack::v1::zone>::__on_zero_shared()   shared_ptr.h:183
  <- MsgPack::Body::~Body()                                              msgpack.h:594
  <- DocIndexer::~DocIndexer()                                           handler.h:344
  <- restore_database_view(Request&) (.resume)                           search_views.cc:1774
```

(192 of 238 are in `madvise` itself) and one is the scrape (below). `DocIndexer`'s `_results` holds one `MsgPack` map
per object (`handler.cc:2405` `resize(idx + 1, MsgPack::MAP())`, `:2422` and `:2459` fill it); the response shares them,
so `~DocIndexer` is the last owner and frees all 5,000 trees at the end of the request, in one turn, with no checkpoint.
A GET that arrives inside the turn waits for it: a 18 ms turn every 0.74 s covers 2.4% of the time, so the p99 sits at
about the turn's length.

What else is on the loop during a RESTORE (`sample`, 2,983 busy loop samples; shares of them, inclusive):

| On the loop | Share |
|---|---:|
| `Admitted::~Admitted` (the prepared objects freed after each range, `handler.cc` `run.clear()`) | 19.3% |
| `MsgPack::_init_map`: the result map made per object (`record`, `$_3`: 12.1%; `take`, `$_12`: 6.5%) | 19.7% |
| `Request::~Request` (the parser's objects and `unpacker`) | 7.4% |
| `DocIndexer::~DocIndexer` | 7.0% |
| of all of it, `madvise` (leaf) | 15.4% |
| `Shard::replace` / `apply_slice` / `replace_by_key` | 1.3% |
| the parse hand-off (`feed_whole_body`, `parse_ahead`) | 0.7% |
| `Schema::index`, `prepare_plain`, `finalise_write` | 0% |

The loop's RESTORE time is allocating and freeing the objects' `MsgPack` trees, which workers build and the loop
frees; only the last free is long enough (one per chunk, 5,000 trees) to show as a turn over 3 ms.

The existing design. Prepare as a chore (`phase7c.md` D12, `x7-loop-stalls.md` fix (a)) and the per-shard apply chores are
already on master: `DocIndexer::index` pulls rounds in `flush` chores (`handler.cc:2813` `pull_round`, `:2352`
`prepare_plain`) and applies in chores (`:2684` `apply_slice`). So `reactor-offload.md`'s "prepare and apply run per object
on the reactor" no longer holds on master; the design cannot remove this stall (0 ms of it is prepare or apply).

Fix, the smallest that moves it: free the results in slices. `harness/restore_stall_slice_results.patch` (11 lines, one
TU, `search_views.cc`) pops `_results` 32 at a time with `co_await request.budget->checkpoint()` between, after the
response is written and before `indexer` goes out of scope. ABBA, strict, trivial GET during the RESTORE phase:

| Arm | p99 | p99.9 | max | loop lag p99.9 | RESTORE docs/s |
|---|---:|---:|---:|---:|---:|
| master (1) | 24.14 | 33.08 | 37.1 | 20,480 us | 6,734 |
| slice (2) | 5.59 | 15.48 | 17.5 | 2,560 us | 6,671 |
| slice (3) | 5.77 | 13.30 | 15.1 | 2,048 us | 6,718 |
| master (4) | 22.40 | 32.82 | 41.7 | 20,480 us | 6,664 |

p99 4.1x lower (23.3, the masters' mean, to 5.7), loop lag p99.9 8-10x lower, RESTORE throughput 6,671-6,718 docs/s against
6,664-6,734 (inside the masters' own 1.0% spread). The spin probe on the slice build sees no loop turn over 3 ms in the RESTORE phase (no turn over 3 ms, against 27 on master). The p99 is still above the 2.2 ms A/A; what remains is not a turn on this loop (none over 3 ms), and was not
attributed: the trivial GET reads its stored record through the `WorkPool` while the RESTORE's chores keep the 16 workers
busy (2,400-2,700 busy samples each in the sampled run), a candidate this run did not test.

The proper change is the same idea without `const_cast`: a `DocIndexer::release_results()` task that frees in slices, called
before the closing response; or a `_results` that holds a status and an id per object (8-16 bytes) and builds the
response array once, so no per-object map exists at all. The first is 10 lines and needs a test in
`tests/test_loop_yield.cc` that fails without it (a RESTORE chunk's loop turns stay under a bound); the second changes
`record` and the response writer. The sliced free also applies to `Admitted::~Admitted` and the parser's objects, which
are 19% and 7% of the loop's samples but not yet long turns.

## Fix: the results in one buffer

Why 5,000 results cost 10-25 ms to free. Every `MsgPack` value owns a `msgpack::zone`, and a zone `malloc`s its first
8,192-byte chunk when it is made (`msgpack/detail/cpp11_zone.hpp`, `MSGPACK_ZONE_CHUNK_SIZE`). `_results` held one
`MsgPack` per object, so a 5,000-object chunk held 5,000 zones, 41 MB of 8 KB blocks, and `~DocIndexer` freed them in
one turn; Darwin's `free` of an 8 KB small-zone block returns its pages with `madvise`, which is the 81%. Recording
made two more zones per object on the loop (the `MsgPack::MAP()` placeholder of `admit` and the `MsgPack result` of
`record`), freed one at a time.

The change (`src/database/handler.{h,cc}`): `DocIndexer` keeps its results as MessagePack bytes appended to one
`std::string` (`_packed`) and an (offset, size) per object (`_at`); `record`, `apply` and the failure path pack each
value from the `msgpack::object` the `MsgPack` held before, and `results()` builds the response's array once, by
unpacking the concatenation into one `MsgPack`. No per-object `MsgPack` exists for a result, so the chunk's end frees
two blocks instead of 5,000 zones, and recording allocates nothing per object. Slicing the free was not needed: the
cost was the allocation, not its timing. The CLI restore, which keeps its results for the whole file, holds tens of
bytes per document instead of a zone each (arithmetic, not measured: 8 KB per document before, 8 GB per million).

The RESTORE responses are unchanged: `harness/restore_stall_items.py` sends the same 32 RESTOREs (streamed and array
bodies; given, new numeric and repeated ids; one and three shards; echo, comments and neither; failing and script
objects) to a node of each binary, in JSON and in MessagePack, and all 64 responses match byte for byte but `took`
(96 failed items among them).

The first exclusive A/B of this fix (job 261116-140557-b42752, Xapiand:6c51552ef against master Xapiand:ceb058569, ABBA):
RESTORE-phase p99 22.0/22.4 ms on master, 8.2/8.0 ms on the fix; loop lag p99.9 20.5 ms to 2.6 ms; 6,623 to 6,975
docs/s. The 8 ms left is the next section.

## Fix, round 2: the 8 ms left

Three things, measured on the fix above (screening runs under `quiet strict` beside light holds, so a few ms of
noise; raw output under `.scratch/e1`-`e6` of the quest's worktree, not committed):

1. The loop still built and freed each object's MsgPack tree. Spin probe at 1 ms: 1,207-1,686 loop turns over 1 ms
   in a 20 s RESTORE phase, nearly all 1-2 ms; their stack captures are `DocIndexer::index`'s `take`
   (`reads_stored` calls `MsgPack::find`, whose `_init_map` builds the whole child map of the object on the loop:
   182 of 290 captures) and `Admitted::~Admitted` under `run.clear()` (48; 22.5% of the loop's `sample` busy
   samples, half of it `madvise`).
2. Those turns are not the 8 ms. A GET that touches no index (`/:none`, a 400 from the URL resolver) has the same
   p99 during the RESTORE as the document GET (8.3-8.9 against 8.3-8.6 ms), while the loop's lag probe stays at
   p99.9 1.3-2.6 ms.
3. The 8 ms is the probe's own client. The trivial GETs and the RESTORE client shared one Python event loop, and
   building and sending each 5,000-document body stalls that loop: a 1 ms ticker in the client wakes 10-11 ms
   late at p99.9 during the RESTORE phase (0.5 ms in the trivial phase). With the RESTORE client in a process of
   its own (`harness/restore_stall_probe.py` now runs it so), the same binaries measure:

| Binary | trivial p99 (A/A) | RESTORE-phase p99 | p99.9 | max | docs/s |
|---|---:|---:|---:|---:|---:|
| fix of round 1, client inline (earlier runs) | 1.1-1.5 | 8.3-10.5 | 15-22 | 20-32 | 5,700-7,100 |
| fix of round 1, client in its own process | 1.06-1.10 | 2.25-2.86 | 2.9-4.1 | 5.0-6.2 | 5,710-6,472 |
| round 2, client in its own process | 1.09 | 1.71 | 2.62 | 4.33 | 6,508 |
| master, client in its own process | 0.87 | 11.25 | 20.94 | 25.7 | 6,409 |

Round 2 (`src/database/handler.cc`) takes the per-object tree off the loop: `reads_stored` reads the map's raw
pairs (the last of a repeated key wins, as `find`; a key that is not a string throws as before), the run's objects
are freed in a chore of the RESTORE's group, and `index` checkpoints after each object it takes, so a batch of any
size admits in slices. Spin at 1 ms: 72-115 turns over 1 ms in the RESTORE phase, against 1,686 on round 1. The
few turns over 4 ms left hold no RESTORE frame: their captures are the spin probe's own `fprintf` to `node.log`.
`tests/test_loop_yield.cc` `test_a_restores_objects_are_freed_off_its_loop` counts the blocks the loop frees in a
2,000-object RESTORE.

The deciding numbers are the exclusive A/B jobs in the quest's journal.

## 2. The lag probe's 49-82 ms

`LagProbe` (`reactor_lag.h`) re-arms a `steady_timer` and records how late each wake ran. Is its max a floor or a stall?

| Test | Result |
|---|---|
| `lag_floor` (the probe alone on an idle `io_context`, no node), 100 ms idle mode, 20 s | p99 at most 0.9-2.0 ms, max 2.05 ms |
| same, 1 ms busy mode (activity every 5 ms) | p99 at most 0.38 ms, max 0.47 ms |
| same, 8 CPU-burning threads and the spinner beside | idle mode max 0.5 ms, busy mode max 3.0 ms (host contention is the probe's real floor) |
| the sleep-free probe, same `io_context` | iteration gaps 0.13-0.18 ms; the timer's lateness with a spinner awake: 0.05-0.15 ms |
| the node, spin probe, 60 s with no request at all | 0 loop turns over 3 ms, 0 stall captures |
| the node, spin probe, idle, trivial and trivial3 windows | 0 turns over 3 ms in each |
| the node, `GET /:metrics` x20 | median 56.2 ms (min 45.9, max 58.5); `GET /` median 0.42 ms |
| same at a 1,024-descriptor limit | `/:metrics` median 0.7 ms |
| `fd_poll_cost` (the same loop alone) at limit 1,024 / 10,240 / 65,536 / 262,144 / 1,048,576 | 0.04 / 0.42 / 2.8 / 10.8 / 42.0 ms |

So the floor is 2 ms, not 49-82 ms, and nothing periodic runs on a quiet loop. The stall is the scrape. The watchdog
captured it (375 of 383 scrape captures were in `poll`):

```
poll (libsystem_kernel) <- get_open_files_per_proc()  system.cc:131-144   (the pollfd sweep up to RLIMIT_NOFILE)
  <- XapiandManager::server_metrics_impl()            src/manager.cc:1315
  <- metrics_view(Request&)                           src/server/search_views.cc:831
```

and the sleep-free probe's 72-98 ms gaps fall on the scrapes (and on nothing else). The harness scrapes at each phase's
start and end; the probe counts the scrape's lateness at its next tick, after the handler returned and after the
metrics snapshot that the same response contains, so the stall lands in the next scrape's window: every busy phase
reports it as `max_us_window` (49,152 us bucket, 41-49 ms late). An idle phase shows it only when a 100 ms tick happened to
fall inside the 52 ms turn (a1 idle: 40,960 us; a2 idle: 5,120 us). The node raises a low `RLIMIT_NOFILE` itself, so starts at 10,240 and 65,536 still scraped in 51.9 and 53.0 ms. In production the same turn holds the serving loop
once per Prometheus scrape; at the default limit (1,048,576 here) that is 52-56 ms of the loop's time each.

Smallest fix: `get_open_files_per_proc()` is in the pinned `Kronuz/system` library, called from `manager.cc:1315`: count open
descriptors by listing them (`/proc/self/fd` on Linux, `proc_pidinfo(PROC_PIDLISTFDS)` on macOS, `/dev/fd` where mounted)
instead of polling up to the limit: cost proportional to the open descriptors (about 40) instead of the limit. Or keep the
call and run it as a chore, with the gauge cached for a scrape interval. Not measured; the cost model is `fd_poll_cost`'s.

## Limits

- One macOS host (Darwin's `free` returns memory with `madvise`; 81% of the long turn is that call). The 10-25 ms is
  Darwin's; a glibc or FreeBSD free of the same 5,000 trees was not measured, so the sliced free is a bounded-turn fix
  everywhere but its p99 gain is only measured here. Linux and arm64 not run.
- 20,000 documents loaded, not 300,000: the RESTORE phase indexes into another index and the trivial GET's p99 does not
  depend on `accounts`' size (A/A 2.2 ms, as in `reactor-offload.md`'s 1.4-2.3 ms on 300,000).
- 5,000-document chunks (27 chunks in 20 s); a chunk of 20,000 would make one longer turn four times less often.
- The stall sampler walks frame pointers from a signal, so it names the frame that held the loop, not the 1.5 ms it
  stalled for; the gap log gives the length. Captures are about one per 1.5 ms of stall, so a turn under 3 ms is not seen.
- The p99 left after the slice (5.6-5.8 ms) is not attributed.
- The `sample` run's latencies (p99 107 ms) are the sampler's, discarded.

## Reproduce

```sh
# patch the pinned reactor's header, then build master
git -C .deps/reactor-src apply ../../harness/restore_stall_reactor_lag.patch
quiet light BUILD 60 "build" -- cmake --build BUILDDIR --target xapiand -j4

c++ -std=c++20 -O2 -DASIO_STANDALONE -I.deps/reactor-src -I.deps/asio-src/asio/include harness/lag_floor.cc -o lag_floor -pthread
c++ -std=c++17 -O2 harness/fd_poll_cost.cc -o fd_poll_cost

quiet strict NAME 28 "restore-stall" -- harness/restore_stall_run.sh MASTER_BIN OUT1        # floor, A/A, spin, sample
quiet strict NAME 28 "restore-stall" -- harness/restore_stall_run2.sh MASTER_BIN SLICE_BIN OUT2   # scrape, quiet, slice ABBA
harness/restore_stall_stalls.py BIN OUT2/spin-master/node.log --probe-log OUT2/spin-master.log --probe-clock monotonic --phase restore
harness/restore_stall_profile.py OUT1/sample/restore.sample.txt
```

Raw output of both holds is under `$HOME/.scratch/restore-stall/out1` and `out2` (not committed; the tables above are
the numbers).
