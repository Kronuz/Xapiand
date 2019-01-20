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

* [**_Metric_**](metrics)

  Aggregations that keep track and compute metrics over a set of documents.

* [**_Bucketing_**](bucket)

  A family of aggregations that build buckets, where each bucket is associated
  with a key and a document criterion. When the aggregation is executed, all
  the buckets criteria are evaluated on every document in the context and when
  a criterion matches, the document is considered to "fall in" the relevant
  bucket. By the end of the aggregation process, we’ll end up with a list of
  buckets - each one with a set of documents that "belong" to it.

The interesting part comes next. Since each bucket effectively defines a
document set (all documents belonging to the bucket), one can potentially
associate aggregations on the bucket level, and those will execute within the
context of that bucket. This is where the real power of aggregations kicks in:
**aggregations can be nested!**

{: .note .info}
**_Sub-aggregations_**<br>
Bucketing aggregations can have _sub-aggregations_ (bucketing or metric). The
sub-aggregations will be computed for the buckets which their parent aggregation
generates. There is no hard limit on the level/depth of nested aggregations (one
can nest an aggregation under a "parent" aggregation, which is itself a
sub-aggregation of another higher-level aggregation).

{: .note .info}
**_Limits_**<br>
Aggregations operate on the `double` representation of the data. As a
consequence, the result may be approximate when running on longs whose absolute
value is greater than 2^53.
