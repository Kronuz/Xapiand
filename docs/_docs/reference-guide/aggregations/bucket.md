---
title: Bucket Aggregations
---

Bucket aggregations don’t calculate metrics over fields like the metrics
aggregations do, but instead, they create buckets of documents. Each bucket is
associated with a criterion (depending on the aggregation type) which determines
whether or not a document in the current context "falls" into it. In other
words, the buckets effectively define document sets. In addition to the buckets
themselves, the bucket aggregations also compute and return the number of
documents that "fell into" each bucket.

Bucket aggregations, as opposed to metrics aggregations, can hold
sub-aggregations. These sub-aggregations will be aggregated for the buckets
created by their "parent" bucket aggregation.

There are different bucket aggregators, each with a different "bucketing"
strategy. Some define a single bucket, some define fixed number of multiple
buckets, and others dynamically create the buckets during the aggregation
process.

Available bucket aggregations:

  * [Filter](filter-aggregation)
  * [Values](values-aggregation)
  * [Terms](terms-aggregation)
  * Date Histogram <sup>*</sup>
  * Date Range <sup>*</sup>
  * Geo-spatial Distance <sup>*</sup>
  * Geo-spatial Trixels <sup>*</sup>
  * [Histogram](histogram-aggregation)
  * Missing value <sup>*</sup>
  * [Range](range-aggregation)
  * IP range <sup>*</sup>
  * Geo-spatial IP <sup>*</sup>

{: .note .unreleased}
**_Unimplemented Features!_**<br>
Features with asterisk haven't yet been implemented...
[Pull requests are welcome!]({{ site.repository }}/pulls)
