# Bulk load: the 2018 Kibana samples, rerun

State: measured. Xapiand master on strata against Elasticsearch 9.5.4, five exclusive trials per arm and dataset.

## Method

`harness/bench/bulk_load.sh ARM --trials 5 --datasets D` (A1), one `exclusive` job per arm and dataset at
Xapiand:29a2aadd7 (master b2cd24fa0 plus the harness). Host: macOS 15.8.1, Intel Core i9-9980HK (8 cores, 16
threads), 32 GiB. Each trial starts the engine fresh on an empty directory, loads one dataset, and stops it; the
time runs from the first request to the durable acknowledgment. Datasets are Kibana's 2018 tutorial samples
(`~/Development/Kronuz/datasets/kibana-2018/`): shakespeare 111,396 documents, logs 14,005, accounts 1,000.

| Arm | What is timed |
|---|---|
| xapiand | `--shards 1`, one `RESTORE /idx/?commit` of `es-D.msgpack`; answered after the strata commit (fsync plus the macOS barrier fsync, docs/design/phase7c.md D7) |
| xapiand-full | the same with `--full-flush-ordering` (F_FULLFSYNC) |
| es | one `_bulk?refresh=wait_for` of `D-2.json`, `index.translog.durability=request`, one shard, no replicas |
| es-chunked | `_bulk` in requests of 5,000 documents, the last with `refresh=wait_for` |

## Results

Seconds are the median of five trials, with the minimum and maximum. Peak RSS and size on disk are medians.

| Dataset | Arm | 2018 s | Today s (min-max) | Docs/s | Peak RSS | Disk | Job |
|---|---|---|---|---|---|---|---|
| shakespeare | xapiand | 15.09 | 5.850 (5.774-6.729) | 19,041 | 1,827 MB | 19.4 MB | 261120-190936-9f9465 |
| shakespeare | xapiand-full | | 5.855 (5.774-6.071) | 19,027 | 1,826 MB | 19.4 MB | 261120-190942-2c3d9d |
| shakespeare | es | 7.90 | 7.917 (7.587-8.148) | 14,070 | 9,786 MB | 45.4 MB | 261120-190947-713aad |
| shakespeare | es-chunked | | 7.728 (7.560-8.764) | 14,414 | 9,846 MB | 45.5 MB | 261120-190951-0ac3cc |
| logs | xapiand | 22.61 | 4.537 (4.442-4.667) | 3,087 | 1,303 MB | 45.6 MB | 261120-190934-5e5b1b |
| logs | xapiand-full | | 4.918 (4.750-4.952) | 2,848 | 1,304 MB | 45.6 MB | 261120-190940-a549e5 |
| logs | es | 11.29 | 8.534 (7.899-11.639) | 1,641 | 10,249 MB | 93.1 MB | 261120-190945-47638c |
| logs | es-chunked | | 8.792 (8.711-8.975) | 1,593 | 9,181 MB | 92.9 MB | 261120-190950-a4cd57 |
| accounts | xapiand | 0.21 | 0.153 (0.142-0.235) | 6,520 | 48 MB | 0.6 MB | 261120-190932-efa931 |
| accounts | xapiand-full | | 0.208 (0.205-0.212) | 4,805 | 42 MB | 0.6 MB | 261120-190938-506908 |
| accounts | es | 0.08 | 1.391 (1.375-1.443) | 719 | 10,088 MB | 0.8 MB | 261120-190944-c56cfa |
| accounts | es-chunked | | 1.359 (1.241-1.392) | 736 | 9,403 MB | 0.8 MB | 261120-190948-e382e6 |

The 2018 column is the April 2018 run on one Mac (Xapiand on Xapian, `:restore`; Elasticsearch `_bulk`), as the
quest records it; that machine and Elasticsearch's acknowledgment point then are not recorded.

## What moved

In 2018 Xapiand loaded about 2× slower than Elasticsearch. Today it loads faster than Elasticsearch on every
dataset: shakespeare 1.35× faster (5.85 s against 7.92 s), logs 1.88× faster (4.54 s against 8.53 s), accounts
9.1× faster (0.153 s against 1.391 s). Against its own 2018 numbers Xapiand is 2.6× faster on shakespeare, 5.0×
on logs and 1.4× on accounts. Its indexes are 57% smaller than Elasticsearch's on shakespeare (19.4 MB against
45.4 MB) and 51% smaller on logs. F_FULLFSYNC costs 8% on logs, 36% on accounts (0.055 s, about one full flush) and nothing measurable on
shakespeare.
Chunking Elasticsearch's requests changes its time by under 4%.

Two numbers need care:

1. Elasticsearch's accounts time (1.39 s against 0.08 s in 2018) is likely `refresh=wait_for` waiting for the
   one-second refresh interval on a cold JVM, not indexing. Unverified; a run with `refresh_interval=-1` and an
   explicit `_refresh` after the load would settle it.
2. Peak RSS: Elasticsearch's 9.2 to 10.2 GB is its default heap, reserved before the load (idle RSS is within
   1% of peak). Xapiand idles at 9 MB and peaks at 1,303 MB on logs and 1,827 MB on shakespeare, about 29× and
   94× its index size on disk. What holds that memory (the request body, per-document buffers, the write
   buffer) is not measured yet; it is a hunt-list row (bulk-load peak RSS).
