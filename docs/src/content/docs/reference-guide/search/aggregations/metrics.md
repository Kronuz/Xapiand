---
title: "Metrics Aggregations"
---

The aggregations in this family compute metrics based on values extracted in one
way or another from the documents that are being aggregated. The values are
extracted from the fields of the document (`_field`); a metric has no script form.

`_min`, `_max`, `_sum`, `_avg`, `_variance`, `_std_deviation`, `_stats`, `_extended_stats`, `_median`
and `_mode` take fields of type `positive`, `integer`, `floating`, `date`, `datetime`, `time` or
`timedelta` (any other type answers `400`, "<type> type is not supported"); `_count` takes every type.

Numeric metrics aggregations are a special type of metrics aggregation which
output numeric values. Some aggregations output a single numeric metric
(e.g. `_avg`) and are called _single-value numeric metrics aggregation_, others
generate multiple metrics (e.g. `_stats`) and are called
_multi-value numeric metrics aggregation_. The distinction between single-value
and multi-value numeric metrics aggregations plays a role when these
aggregations serve as direct sub-aggregations of some bucket aggregations (some
bucket aggregations enable you to sort the returned buckets based on the numeric
metrics in each bucket).

Available metrics aggregations:

  * [Count](count-aggregation)
  * Cardinality <small>🔧</small>
  * [Sum](sum-aggregation)
  * [Average](avg-aggregation)
  * [Min](min-aggregation)
  * [Max](max-aggregation)
  * [Variance](variance-aggregation)
  * [Standard Deviation](std_deviation-aggregation)
  * [Median](median-aggregation)
  * [Mode](mode-aggregation)
  * [Statistics](stats-aggregation)
  * Geo-spatial (bounds) <small>🔧</small>
  * Geo-spatial (centroid) <small>🔧</small>
  * Percentiles <small>🔧</small>
  * Percentiles Rank <small>🔧</small>
  * Scripted <small>🔧</small>
  * [Extended Statistics](extended_stats-aggregation)

:::hint[Unimplemented Features!]{.unimplemented}
Some features haven't yet been implemented...
<br>[Pull requests are welcome!](https://github.com/Kronuz/Xapiand/pulls)
:::

## Structuring

The following snippet captures the structure of aggregations types for metrics:

```json
"<aggregation_name>": {
  "<metrics_aggregation_type>": {
    "_field": "<field_name>",
    ...
  },
  ...
}
```
