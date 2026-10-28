# Nested `_terms` blow-up

Status: landed on `fix/agg-buckets` (unit tests in `tests/test_engine_sort.cc`).

## Cause

Not a per-level engine bug. A bucket aggregation feeds each document to one bucket per value it holds
(`BucketAggregation::aggregate`, `src/aggregations/bucket.h`, via `HandledSubAggregation::_aggregate_*` over
`_handler.values(doc)` in `src/aggregations/metrics.h`), and every bucket builds its own sub-aggregations. Over a
multi-valued field, L nested levels make V^L buckets (V values per document): a document is counted once in each
combination. Both engines do it (`TermsHandler::values` returns every stored value or term). The only bound was the
per-level `_shard_size`, which does not limit the product of the levels. The over-count seen earlier (342 buckets
counted over 300 documents) is the same effect: a document with several values is counted in each of its buckets,
so the sum over a level exceeds the matching documents. This is Elasticsearch's behavior too and is kept: a multi-valued
document counts once in every bucket it belongs to (`test_aggregation_multivalued_counts_each_bucket`). Single-valued fields stay linear (verified on `n`, `m`, `k`).

## Fix

Elasticsearch-style limits, enforced while collecting:

1. `AGGS_MAX_BUCKETS` (65,535): buckets below the outermost level, charged in `BucketAggregation::aggregate` (this
   branch has no `_terms` cap to credit back, so the outermost buckets, bounded by the distinct values, are not charged); over it the search fails with 400 "Aggregations would make more than N buckets".
   The budget lives in `AggregationMatchSpy` and reaches the buckets through a thread-local set in `see()`.
2. `AGGS_MAX_DEPTH` (16): `AggregationMatchSpy::check_depth` refuses deeper nesting up front.

## Measured (300 documents, `v` holding 7 values, `harness/agg_buckets_check.py`, strict hold)

| depth | before time / RSS growth | after |
|---|---|---|
| 4 | 0.16 s / +1 MB | same |
| 5 | 1.05 s / +9..15 MB | same (16,807 leaf buckets) |
| 6 | 8.0 s / +205 MB (xapian), 8.2 s / +194 MB (strata) | 400 in 0.05 s / +30..37 MB |

Growth is about 7.5x per level. `harness/terms_cap_check.py` (200,000 distinct values) still passes on both engines.
Depth 5 is still 1 s: the work is 300 documents x 7^5 paths, which the limits allow.
