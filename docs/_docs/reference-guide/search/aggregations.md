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

{: .note .info }
**_Sub-aggregations_**<br>
Bucketing aggregations can have _sub-aggregations_ (bucketing or metric). The
sub-aggregations will be computed for the buckets which their parent aggregation
generates. There is no hard limit on the level/depth of nested aggregations (one
can nest an aggregation under a "parent" aggregation, which is itself a
sub-aggregation of another higher-level aggregation).

{: .note .info }
**_Limits_**<br>
Aggregations operate on the `double` representation of the data. As a
consequence, the result may be approximate when running on longs whose absolute
value is greater than 2^53.


## Structuring

The following snippet captures the basic structure of aggregations:

```json
"_aggregations": {
    "<aggregation_name>": {
        "<aggregation_type>": {
            <aggregation_body>
        },
        ( "_meta": {  <metadata_body> }, )?
        ( "_aggregations": { (<sub_aggregation>)+ }, )?
    }
    ( "<aggregation_name_2>": { ... }, )*
}
( "_limit": <limit>, )
( "_check_at_least": <check_at_least>, )
```

### Aggregation Name

The `_aggregations` object (the key `_aggs` can also be used) in the JSON
holds the aggregations to be computed.

Each aggregation is associated with a logical `<aggregation_name>` that the user
defines (e.g. if the aggregation computes the average price, then it would make
sense to name it "avg_price"). These logical names will also be used to uniquely
identify the aggregations in the response.

### Aggregation Body

Typically, the first key within the named aggregation body sets the specific
`<aggregation_type>`, which defines it's own `<aggregation_body>`, depending
on the nature of the aggregation (e.g. an _Average aggregation_ on a specific
field will define the _field_ on which the average will be calculated).

### Metadata

{: .note .unimplemented }
**_Unimplemented Feature!_**<br>
This feature hasn't yet been implemented...
[Pull requests are welcome!]({{ site.repository }}/pulls)

At the same level of the aggregation type definition, one can optionally
associate a piece of metadata with individual aggregations at request time
(by using `<metadata_body>`, in `_meta`) that will be returned in place at
response time.

### Nested Aggregations

Also at the same level of the aggregation type definition, one can optionally
define a set of additional nested `_aggregations`, though this only makes
sense if the aggregation you defined is of a _bucketing_ nature. In this
scenario, the `<sub_aggregation>` you define on the bucketing aggregation level
will be computed for all the buckets built by the bucketing aggregation. For
example, if you define a set of aggregations under the range aggregation, the
sub-aggregations will be computed for the range buckets that are defined.

### Values Source

Some aggregations work on values extracted from the aggregated documents.
Typically, the values will be extracted from a specific document field which is
set using the field key for the aggregations.

{: .note .unimplemented }
**_Unimplemented Feature!_**<br>
This feature hasn't yet been implemented...
[Pull requests are welcome!]({{ site.repository }}/pulls)

{{ site.name }} uses the type of the field in the mapping in order to figure out
how to run the aggregation and format the response. However there are two cases
in which {{ site.name }} cannot figure out this information. For those cases,
it is possible to give {{ site.name }} a hint using the `_value_type` option,
which accepts the same values as the index schema (e.g. `string`, `positive`,
`integer`, `datetime`, `boolean`, etc.)

### Query DSL

One can use other Query DSL specific parameters at the same level as the topmost
`_aggregations` key. For example, there are many occasions when aggregations
are required but search hits are not. For these cases the hits can be ignored by
setting the Query DSL `_limit` parameter to zero.
