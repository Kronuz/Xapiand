---
title: Metrics Aggregations
---

The aggregations in this family compute metrics based on values extracted in one
way or another from the documents that are being aggregated. The values are
typically extracted from the fields of the document (using the field data), but
can also be generated using scripts.

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
  * Cardinality <sup>*</sup>
  * [Sum](sum-aggregation)
  * [Average](avg-aggregation)
  * [Min](min-aggregation)
  * [Max](max-aggregation)
  * [Variance](variance-aggregation)
  * [Standard Deviation](std_deviation-aggregation)
  * [Median](median-aggregation)
  * [Mode](mode-aggregation)
  * [Statistics](stats-aggregation)
  * Geo-spatial (bounds) <sup>*</sup>
  * Geo-spatial (centroid) <sup>*</sup>
  * Percentiles <sup>*</sup>
  * Percentiles Rank <sup>*</sup>
  * Scripted <sup>*</sup>
  * [Extended Statistics](extended_stats-aggregation)

{: .note .unreleased}
**_Unimplemented Features!_**<br>
Features with asterisk haven't yet been implemented...
[Pull requests are welcome!]({{ site.repository }}/pulls)
