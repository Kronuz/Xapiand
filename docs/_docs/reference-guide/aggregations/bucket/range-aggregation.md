---
title: Range Aggregation
---

A _multi-bucket_ value source based aggregation that enables the user to define
a set of ranges - each representing a bucket.


## Structuring

The following snippet captures the structure of range aggregations:

```json
"<aggregation_name>": {
  "_range": {
      "_field": "<field_name>",
      ("_keyed": <keyed_boolean>, )?
      "_ranges": [
        ( {
          ( "_key": <key_name>, )?
          "_from": <from_value>,
          "_to": <to_value>
        }, )+
      ]
  },
  ...
}
```

### Field

The `<field_name>` in the `_field` parameter defines the field on which the
aggregation will act upon.

### Response Format

By default, the buckets are returned as an ordered array. Typically, for ranges,
the `_keyed` boolean option is set to `true` so it returns the buckets in the
response in an object keyed by the bucket key name.

### Ranges

During the aggregation process, the values extracted from each document will be
checked against each bucket range and "bucket" the relevant/matching document.

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
        "_keyed": true,
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
        "_keyed": true,
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

### Ordering

By default, the returned buckets are sorted in the same order the ranges were
listed in, though the order behaviour can be controlled using the `_sort`
setting. Supports the same order functionality as explained in
[Bucket Ordering](..#ordering).
