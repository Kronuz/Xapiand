# Source layout: split files

Large sources are split by responsibility into pieces in the same directory, named with the
original file's name as prefix, so CMake's per-directory globs pick them up. Each piece opens with a
one-line comment saying what it holds, and stays under 1,000 lines. A header keeps its path as the
one include callers name; it includes its pieces.

## src/serialize.cc

`Serialize` and `Unserialize` (declared in `src/serialize.h`).

| File | Holds |
|---|---|
| `src/serialize.cc` | `Serialize::MsgPack`, `object`, `serialize` and `string`: the dispatch by a field spec's type |
| `src/serialize_scalar.cc` | the per-type serializers: datetime, time, timedelta, floating, integer, positive, boolean, uuid, and geospatial (ranges, centroids, trixel ids and ranges) |
| `src/serialize_exact.cc` | `Serialize::exact` and its number helpers; defines `serialize_detail::positive_of` |
| `src/serialize_guess.cc` | `isText`, `possiblyUUID`, `isUUID`, `guess_type`, ids, `guess_serialize` |
| `src/serialize_unserialize.cc` | `Unserialize::*` and `get_field_type` |
| `src/serialize_detail.h` | what the pieces share: `UUID_SEPARATOR_LIST`, `UUID_ENCODER`, `serialize_detail::positive_of` |

## src/aggregations/metrics.h

`metrics.h` includes the pieces in this order.

| File | Holds |
|---|---|
| `src/aggregations/metrics_base.h` | `ValuesHandler`, `TermsHandler`, `HandledSubAggregation` |
| `src/aggregations/metrics_simple.h` | `MetricCount`, `MetricSum`, `MetricAvg`, `MetricMin`, `MetricMax` |
| `src/aggregations/metrics_variance.h` | `MetricVariance`, `MetricStdDeviation` |
| `src/aggregations/metrics_quantile.h` | `MetricMedian`, `MetricMode` |
| `src/aggregations/metrics_stats.h` | `MetricStats`, `MetricExtendedStats` |

## src/aggregations/bucket.h

`bucket.h` includes the pieces in this order.

| File | Holds |
|---|---|
| `src/aggregations/bucket_base.h` | `BucketAggregation`, the base of the bucket aggregations |
| `src/aggregations/bucket_terms.h` | `ValuesAggregation`, `TermsAggregation`, `FilterAggregation` |
| `src/aggregations/bucket_numeric.h` | `HistogramAggregation`, `RangeAggregation` |

## src/lua/processor.cc

The Lua script `Processor` (declared in `src/lua/processor.h`).

| File | Holds |
|---|---|
| `src/lua/processor.cc` | `Processor` and this thread's script cache: compiling local and foreign scripts |
| `src/lua/processor_state.cc` | `detail::State`: the metered allocator, count hook, host functions, base chunk, and the library patch's `lua_x*` meters |
| `src/lua/processor_run.cc` | the document conversion between MsgPack and Lua, `run` (one call's resume loop) and `collect` |
| `src/lua/processor_detail.h` | what the pieces share: `detail::State`, `Call`, `current_call`, `printed`, `hook`, `throw_if_condemned`, `run`, `collect` |
