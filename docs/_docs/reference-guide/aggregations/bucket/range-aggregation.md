---
title: Range Aggregation
---

A _multi-bucket_ value source based aggregation that enables the user to define
a set of ranges - each representing a bucket. During the aggregation process,
the values extracted from each document will be checked against each bucket
range and "bucket" the relevant/matching document. Note that this aggregation
includes the `_from` value and excludes the `_to` value for each range.

Assuming the data consists of documents representing bank accounts, as shown in
the sample dataset of [Exploring Your Data]({{ '/docs/exploring/' | relative_url }}#sample-dataset)
section, the following snippet "buckets" the bank accounts based on ranges
relative to their `balance`:

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
        "_ranges": [
          { "_to": 2000 },
          { "_from": 2000, "_to": 3500 },
          { "_from": 3500 }
        ]
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
  "#aggregations": {
    "_doc_count": 1000,
    "balances_by_range": {
      "..2000.0": {
        "_doc_count": 318
      },
      "2000.0..3500.0": {
        "_doc_count": 520
      },
      "3500.0..": {
        "_doc_count": 162
      }
    }
  },
  ...
}
```


### Keyed Response

It is possible to customize the key associated with each bucket in each range:

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
        "_ranges": [
          { "_key": "poor", "_to": 2000 },
          { "_key": "average", "_from": 2000, "_to": 3500 },
          { "_key": "rich", "_from": 3500 }
        ]
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
  "#aggregations": {
    "_doc_count": 1000,
    "balances_by_range": {
      "poor": {
        "_doc_count": 318
      },
      "average": {
        "_doc_count": 520
      },
      "rich": {
        "_doc_count": 162
      }
    }
  },
  ...
}
```


## Sub Aggregations

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
        "_ranges": [
          { "_key": "poor", "_to": 2000 },
          { "_key": "average", "_from": 2000, "_to": 3500 },
          { "_key": "rich", "_from": 3500 }
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
  "#aggregations": {
    "_doc_count": 1000,
    "balances_by_range": {
      "poor": {
        "_doc_count": 318,
        "age_stats": {
          "_count": 318,
          "_min": 20.0,
          "_max": 40.0,
          "_avg": 30.166666666666669,
          "_sum": 9593.0
        }
      },
      "average": {
        "_doc_count": 520,
        "age_stats": {
          "_count": 520,
          "_min": 20.0,
          "_max": 40.0,
          "_avg": 29.892307692307694,
          "_sum": 15544.0
        }
      },
      "rich": {
        "_doc_count": 162,
        "age_stats": {
          "_count": 162,
          "_min": 20.0,
          "_max": 40.0,
          "_avg": 30.228395061728397,
          "_sum": 4897.0
        }
      }
    }
  },
}
```
