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
  * Date Histogram <small>🔧</small>
  * Date Range <small>🔧</small>
  * Geo-spatial Distance <small>🔧</small>
  * Geo-spatial Trixels <small>🔧</small>
  * [Histogram](histogram-aggregation)
  * Missing value <small>🔧</small>
  * [Range](range-aggregation)
  * IP range <small>🔧</small>
  * Geo-spatial IP <small>🔧</small>

{: .note .unimplemented }
**_Unimplemented Features!_**<br>
Some features haven't yet been implemented...
[Pull requests are welcome!]({{ site.repository }}/pulls)


## Structuring

The following snippet captures the structure of aggregations types for buckets:

```json
"<aggregation_name>": {
  "<bucket_aggregation_type>": {
    ( "_sort": {  <sort_body> }, )?
    ( "_limit": <limit_count>, )?
    ( "_min_doc_count": <min_doc_count>, )?
    ( "_keyed": <keyed_boolean>, )?
    ...
  },
  ...
}
```

### Ordering

The order of the buckets can be customized by setting a `<sort_body>` in the
`_sort` setting. By default each bucket type has different ordering (e.g.
Histogram Aggregation orders its returned buckets by their key ascending).
It is possible to change this default behaviour as documented below:

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


#### Ordering by Sub Aggregations

Ordering the buckets by **single value** metrics sub-aggregation (identified by
the aggregation name):

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
        "_field": "contact.state",
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


Ordering the buckets by **multi value** metrics sub-aggregation (identified by
the aggregation name):

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
        "_field": "contact.state",
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


#### Deep Ordering

{: .note .unimplemented }
**_Unimplemented Feature!_**<br>
This feature hasn't yet been implemented...
[Pull requests are welcome!]({{ site.repository }}/pulls)

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
        "_field": "contact.state",
        "_sort": { "cities.*.balance_stats._max": "asc" }
      },
      "_aggs": {
        "cities": {
          "_values": {
            "_field": "contact.city"
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


### Limit

The maximum number of buckets allowed in a single response is not currently
hard limited, but the default is 10,000 buckets. The `<limit_count>` in the
`_limit` option is a positive integer number used for changing this default.


### Response Format
By default, the buckets are returned as an ordered array. It is also possible
to request the response as an object keyed by the buckets keys by using the
`_keyed` boolean option:

{% capture req %}

```json
POST /bank/:search?pretty

{
  "_query": "*",
  "_limit": 0,
  "_check_at_least": 1000,
  "_aggs": {
    "balances": {
      "_histogram": {
        "_field": "balance",
        "_interval": 1000,
        "_keyed": true
      }
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

Response:

```json
  "aggregations": {
    "_doc_count": 1000,
    "balances": {
      "0.0": {
        "_doc_count": 55
      },
      "1000.0": {
        "_doc_count": 329
      },
      "2000.0": {
        "_doc_count": 286
      },
      "3000.0": {
        "_doc_count": 294
      },
      "4000.0": {
        "_doc_count": 1
      },
      "5000.0": {
        "_doc_count": 1
      },
      "6000.0": {
        "_doc_count": 4
      },
      "7000.0": {
        "_doc_count": 12
      },
      "10000.0": {
        "_doc_count": 9
      },
      "12000.0": {
        "_doc_count": 1
      }
    }
  }, ...
```


### Minimum Document Count

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


### Filtering Values

{: .note .unimplemented }
**_Unimplemented Feature!_**<br>
This feature hasn't yet been implemented...
[Pull requests are welcome!]({{ site.repository }}/pulls)

It is possible to filter the values for which buckets will be created. This can
be done using the include and exclude parameters which are based on regular
expression strings or arrays of exact values. Additionally, include clauses can
filter using partition expressions.


### Collect Mode

{: .note .unimplemented }
**_Unimplemented Feature!_**<br>
This feature hasn't yet been implemented...
[Pull requests are welcome!]({{ site.repository }}/pulls)

Deferring calculation of child aggregations

For fields with many unique terms and a small number of required results it can
be more efficient to delay the calculation of child aggregations until the top
parent-level aggs have been pruned. Ordinarily, all branches of the aggregation
tree are expanded in one _depth-first_ pass and only then any pruning occurs.
In some scenarios this can be very wasteful and can hit memory constraints.


### Missing Value

{: .note .unimplemented }
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
    "gender": {
      "_values": {
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


## Sub Aggregations

Side-by-side the `<bucket_aggregation_type>`, an additional `_aggregations`
object can be added, to nest other sub-aggregations.

The following example, not only "bucket" the documents to the different buckets,
but also computes statistics over the ages of account holders in each balance
range:

{% capture req %}

```json
POST /bank/:search?pretty

{
  "_query": "*",
  "_limit": 0,
  "_check_at_least": 1000,
  "_aggs": {
    "balances_by_range": {
      "_range": {
        "_field": "balance",
        "_keyed": true,
        "_ranges": [
          { "_key": "poor", "_to": 2000 },
          { "_key": "average", "_from": 2000, "_to": 4000 },
          { "_key": "rich", "_from": 4000 }
        ]
      },
      "_aggs": {
        "age_stats": {
          "_stats": {
            "_field": "age"
          }
        }
      }
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

Response:

```json
{
  "aggregations": {
    "_doc_count": 1000,
    "balances_by_range": {
      "poor": {
        "_doc_count": 384,
        "age_stats": {
          "_count": 384,
          "_min": 17.0,
          "_max": 44.0,
          "_avg": 28.817708333333333,
          "_sum": 11066.0
        }
      },
      "average": {
        "_doc_count": 580,
        "age_stats": {
          "_count": 580,
          "_min": 17.0,
          "_max": 42.0,
          "_avg": 30.387931034482759,
          "_sum": 17625.0
        }
      },
      "rich": {
        "_doc_count": 36,
        "age_stats": {
          "_count": 36,
          "_min": 20.0,
          "_max": 44.0,
          "_avg": 37.52777777777778,
          "_sum": 1351.0
        }
      }
    }
  }, ...
}
```


## Mixing field types

{: .note .warning }
**_Warning_**<br>
When aggregating on multiple indices the type of the aggregated field may not be
the same in all indices. Some types are compatible with each other (positive
integer and float) but when the types are a mix of decimal and non-decimal
number the terms aggregation will promote the non-decimal numbers to decimal
numbers. This can result in a loss of precision in the bucket values.
