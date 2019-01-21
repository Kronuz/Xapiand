---
title: Metrics Aggregations
---

{: .note .construction}
**_TODO:_** This section is a work in progress...

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
  * Sum
  * [Average](avg-aggregation)
  * [Min](min-aggregation)
  * [Max](max-aggregation)
  * Variance
  * Standard deviation
  * Median
  * Mode
  * Statistics
  * Geo-spatial (bounds) <sup>*</sup>
  * Geo-spatial (centroid) <sup>*</sup>
  * Percentiles <sup>*</sup>
  * Percentiles rank <sup>*</sup>
  * Scripted <sup>*</sup>
  * Extended statistics


---

<sup>*</sup> Not yet implemented.
