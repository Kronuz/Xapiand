---
title: Aggregations
---

The aggregations framework helps provide aggregated data based on a search query.
It is based on simple building blocks called aggregations, that can be composed
in order to build complex summaries of the data.

An aggregation can be seen as a unit-of-work that builds analytic information
over a set of documents. The context of the execution defines what this document
set is (e.g. a top-level aggregation executes within the context of the executed
query/filters of the search request).

There are many different types of aggregations, each with its own purpose and
output. To better understand these types, it is often easier to break them into
four main families:

* Bucket Aggregations

	A family of aggregations that build buckets, where each bucket is associated
	with a key and a document criterion. When the aggregation is executed, all
	the buckets criteria are evaluated on every document in the context and when
	a criterion matches, the document is considered to "fall in" the relevant
	bucket. By the end of the aggregation process, we’ll end up with a list of
	buckets - each one with a set of documents that "belong" to it.


* Metrics Aggregations

	Aggregations that keep track and compute metrics over a set of documents.

The interesting part comes next. Since each bucket effectively defines a
document set (all documents belonging to the bucket), one can potentially
associate aggregations on the bucket level, and those will execute within the
context of that bucket. This is where the real power of aggregations kicks in:
**aggregations can be nested!**

{: .note .info}
**_sub-aggregations_**<br>
Bucketing aggregations can have _sub-aggregations_ (bucketing or metric). The
sub-aggregations will be computed for the buckets which their parent aggregation
generates. There is no hard limit on the level/depth of nested aggregations (one
can nest an aggregation under a "parent" aggregation, which is itself a
sub-aggregation of another higher-level aggregation).

{: .note .info}
**_limits_**<br>
Aggregations operate on the double representation of the data. As a consequence,
the result may be approximate when running on longs whose absolute value is
greater than 2^53.


## Metrics Aggregations

The aggregations in this family compute metrics based on values extracted in one
way or another from the documents that are being aggregated. The values are
typically extracted from the fields of the document (using the field data), but
can also be generated using scripts.

Numeric metrics aggregations are a special type of metrics aggregation which
output numeric values. Some aggregations output a single numeric metric
(e.g. avg) and are called single-value numeric metrics aggregation, others
generate multiple metrics (e.g. stats) and are called multi-value numeric
metrics aggregation. The distinction between single-value and multi-value
numeric metrics aggregations plays a role when these aggregations serve as
direct sub-aggregations of some bucket aggregations (some bucket aggregations
enable you to sort the returned buckets based on the numeric metrics in each
bucket).


## Bucket Aggregations

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


{: .note .unreleased}
**_TODO:_** Work in progress...
