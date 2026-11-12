> Status: CURRENT (2026-11-11) the cost of `Shard::view()` per call at 1 and 8 threads on one index, for x7-gate A22 (X3-4).

# Shard::view() per call

`engine::strata::Shard::view()` acquires the shard's published generation for a read (`check_read`, `refresh_for_read`, a reader
acquire) and the returned `Viewed` releases it on destruction. The benchmark times acquire plus release, nothing else.

## Conditions

- Commit: `Xapiand:ff03d4e` (the quest branch `quest/xapiand/x7-view-bench`; the benchmark source is unchanged after it, later commits are
  journal and this note). Release build (`-DCMAKE_BUILD_TYPE=Release -DLTO=OFF`).
- Host: Intel Core i9-9980HK (8 cores, 16 threads), 32 GiB, macOS 15.8.1 x86_64. One `quiet strict` job (campaign job 261111-175937-76ae57).
- Index: 10,000 documents, one shard, committed; read handles opened with `open_local(path, false, false)`.
- Rows: `shared` = every thread calls `view()` on one `Shard` object; `handles` = every thread has its own read handle of the same index
  (handles of one index share its core, so both rows hit the same reader state).
- A sample is the mean of a batch of 1000 calls (a clock read costs about as much as the call). 300 batches per thread per round, 20
  rounds after a warm-up round per row; rounds interleave the four rows, so host drift hits all alike.

## Command

```sh
cmake -S . -B build-bench -DFETCHCONTENT_BASE_DIR="$PWD/.deps" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF -DLTO=OFF
cmake --build build-bench --target xapiand_bench_shard_view -j4
quiet strict NAME 10 "Shard::view()" -- build-bench/bin/xapiand_bench_shard_view --rounds 20
```

## Results

ns per call; "round median" is the spread of the 20 per-round medians; throughput is all threads together, median round.

| Mode | Threads | Median | p90 | Round median min..max | Mcalls/s |
|---|---|---|---|---|---|
| shared | 1 | 18.6 | 18.9 | 18.2..20.2 | 52.5 |
| shared | 8 | 20.3 | 29.8 | 19.7..22.8 | 298.2 |
| handles | 1 | 18.6 | 19.0 | 18.2..19.8 | 52.2 |
| handles | 8 | 21.4 | 29.8 | 19.7..23.4 | 300.3 |

## Magnitude

A call costs about 19 ns on one thread. At 8 threads the median call costs 9% (shared) to 15% (handles) more, and the p90 call 57% more
(29.8 against 18.9 ns): contention on the reader count is visible in the tail, not in the middle. Aggregate throughput scales to 5.7x
the one-thread rate on 8 threads (298 against 52.5 Mcalls/s), 71% of linear. On this host `view()` is not a limit: a read that holds one
costs microseconds elsewhere, so 20 ns is under 1% of it. Limits of the number: x86_64 macOS only (no Linux or arm64 row), Release
without LTO, an idle index (no concurrent writer, refresh or merge).
