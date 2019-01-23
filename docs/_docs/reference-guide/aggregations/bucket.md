---
title: Bucket Aggregations
---

Bucket aggregations don't calculate metrics over fields like the metrics
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


## Structuring Bucket Aggregations

The following snippet captures the structure of aggregations types for buckets:

```json
"_aggregations": {
    "<aggregation_name>": {
        "<bucket_aggregation_type>": {
            "_field": "<field_name>"
            [,"_sort": {  [<sort_body>] } ]?
            [,"_limit": <limit_count>]?
            [,"_min_doc_count": <min_doc_count>]?
        }
        ...
    }
    ...
}
...
```

#### Field

The `<field_name>` in the `"_field"` parameter defines the field on which the
aggregation will act upon.

#### Sorting Buckets

The order of the buckets can be customized by setting a `<sort_body>` in the
`"_sort"` parameter. By default, the buckets are ordered by their `_doc_count`
descending. It is possible to change this behaviour as documented below:

Ordering the buckets by their document count in an ascending manner:

{% capture req %}

```json
POST /bank/:search?pretty

{
  "_query": "*",
  "_limit": 0,
  "_check_at_least": 1000,
  "_aggs": {
    "fruits": {
      "_values": {
        "_field": "favoriteFruit",
        "_sort": { "_doc_count": "asc" }
      }
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}


Ordering the buckets alphabetically by their keys in an ascending manner:

{% capture req %}

```json
POST /bank/:search?pretty

{
  "_query": "*",
  "_limit": 0,
  "_check_at_least": 1000,
  "_aggs": {
    "fruits": {
      "_values": {
        "_field": "favoriteFruit",
        "_sort": { "_key": "asc" }
      }
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}


Ordering the buckets by single value metrics sub-aggregation (identified by the
aggregation name):

{% capture req %}

```json
POST /bank/:search?pretty

{
  "_query": "*",
  "_limit": 0,
  "_check_at_least": 1000,
  "_aggs": {
    "balance_by_state": {
      "_values": {
        "_field": "state",
        "_sort": { "max_balance_count._max": "asc" }
      },
      "_aggs": {
        "max_balance_count": {
          "_max": {
            "_field": "balance"
          }
        }
      }
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}


Ordering the buckets by multi value metrics sub-aggregation (identified by the
aggregation name):

{% capture req %}

```json
POST /bank/:search?pretty

{
  "_query": "*",
  "_limit": 0,
  "_check_at_least": 1000,
  "_aggs": {
    "balance_by_state": {
      "_values": {
        "_field": "state",
        "_sort": { "balance_stats._max": "asc" }
      },
      "_aggs": {
        "balance_stats": {
          "_stats": {
            "_field": "balance"
          }
        }
      }
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}


It is also possible to order the buckets based on a "deeper" aggregation in the
hierarchy. This is supported as long as the aggregations path are of a
_single-bucket_ type, where the last aggregation in the path may either be a
_single-bucket_ one or a metrics one. If it's a _single-bucket_ type, the order
will be defined by the number of docs in the bucket (i.e. `_doc_count`), in case
it's a metrics one, the same rules as above apply (where the path must indicate
the metric name to sort by in case of a multi-value metrics aggregation, and in
case of a single-value metrics aggregation the sort will be applied on that
value).

{% capture req %}

```json
POST /bank/:search?pretty

{
  "_query": "*",
  "_limit": 0,
  "_check_at_least": 1000,
  "_aggs": {
    "states": {
      "_values": {
        "_field": "state",
        "_sort": { "cities.*.balance_stats._max": "asc" }
      },
      "_aggs": {
        "cities": {
          "_values": {
            "_field": "city"
          },
          "_aggs": {
            "balance_stats": {
              "_stats": {
                "_field": "balance"
              }
            }
          }
        }
      }
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

#### Limiting Buckets

The `<limit_count>` in the `"_limit"` parameter is a positive integer number
used for limiting the number of returned buckets.

#### Minimum Document Count

It is possible to only return terms that match more than a configured number of
hits using the `_min_doc_count` option:

{% capture req %}

```json
POST /bank/:search?pretty

{
  "_query": "*",
  "_limit": 0,
  "_check_at_least": 1000,
  "_aggs": {
    "employers": {
      "_values": {
        "_field": "employer",
        "_min_doc_count": 5
      }
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

The above aggregation would only return tags which have been found in 5 hits
or more. Default value is 1.


#### Filtering Values

{: .note .unreleased}
**_Unimplemented Feature!_**<br>
This feature hasn't yet been implemented...
[Pull requests are welcome!]({{ site.repository }}/pulls)

It is possible to filter the values for which buckets will be created. This can
be done using the include and exclude parameters which are based on regular
expression strings or arrays of exact values. Additionally, include clauses can
filter using partition expressions.


#### Collect Mode

{: .note .unreleased}
**_Unimplemented Feature!_**<br>
This feature hasn't yet been implemented...
[Pull requests are welcome!]({{ site.repository }}/pulls)

Deferring calculation of child aggregations

For fields with many unique terms and a small number of required results it can
be more efficient to delay the calculation of child aggregations until the top
parent-level aggs have been pruned. Ordinarily, all branches of the aggregation
tree are expanded in one _depth-first_ pass and only then any pruning occurs.
In some scenarios this can be very wasteful and can hit memory constraints.


#### Missing Value

{: .note .unreleased}
**_Unimplemented Feature!_**<br>
This feature hasn't yet been implemented...
[Pull requests are welcome!]({{ site.repository }}/pulls)

The `_missing` parameter defines how documents that are missing a value should
be treated. By default they will be ignored but it is also possible to treat
them as if they had a value.


{% capture req %}

```json
POST /bank/:search?pretty

{
  "_query": "*",
  "_limit": 0,
  "_check_at_least": 1000,
  "_aggs": {
    "avg_gender": {
      "_avg": {
        "_field": "gender",
        "_missing": "N/A"
      }
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

Documents without a value in the `gender` field will fall into the same bucket
as documents that have the value `"N/A"`.


## Mixing field types

{: .note .warning}
**_Warning_**<br>
When aggregating on multiple indices the type of the aggregated field may not be
the same in all indices. Some types are compatible with each other (positive
integer and float) but when the types are a mix of decimal and non-decimal
number the terms aggregation will promote the non-decimal numbers to decimal
numbers. This can result in a loss of precision in the bucket values.
