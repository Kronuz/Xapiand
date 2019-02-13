---
title: Histogram Aggregation
short_title: Histogram
---

A _multi-bucket_ values source based aggregation that can be applied on numeric
values extracted from the documents. It dynamically builds fixed size (a.k.a.
interval) buckets over the values. For example, if the documents have a field
that holds a balance (numeric), we can configure this aggregation to dynamically
build buckets with interval `500` (in case of balance it may represent $500).
When the aggregation executes, the balance field of every document will be
evaluated and will be rounded down to its closest bucket - for example, if the
balance is `3200` and the bucket size is `500` then the rounding will yield
`3000` and thus the document will "fall" into the bucket that is associated with
the key `3000`. To make this more formal, here is the rounding function that is
used:

```cpp
bucket_key = floor((value - _shift) / _interval) * _interval + _shift;
```


## Structuring

The following snippet captures the structure of histogram aggregations:

```json
"<aggregation_name>": {
  "_histogram": {
      "_field": "<field_name>",
      "_interval": "<interval>",
      ( "_shift": <shift> )?
  },
  ...
}
```

Also supports all other functionality as explained in [Bucket Aggregations](..#structuring).

### Field

The `<field_name>` in the `_field` parameter defines the field on which the
aggregation will act upon.

### Interval

The `_interval` must be a positive decimal, while the `_shift` must be a decimal
in `[0, _interval)` (a decimal greater than or equal to `0` and less than
`_interval`)

Assuming the data consists of documents representing bank accounts, as shown in
the sample dataset of [Exploring Your Data]({{ '/docs/exploring/' | relative_url }}#sample-dataset)
section, the following snippet "buckets" the bank accounts based on their
`balance` by interval of `500`:

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
        "_interval": 1000
      }
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

And the following may be the response:

```json
  "#aggregations": {
    "_doc_count": 1000,
    "balances": [
      {
        "_doc_count": 55,
        "_key": "0.0"
      },
      {
        "_doc_count": 329,
        "_key": "1000.0"
      },
      {
        "_doc_count": 286,
        "_key": "2000.0"
      },
      {
        "_doc_count": 294,
        "_key": "3000.0"
      },
      {
        "_doc_count": 1,
        "_key": "4000.0"
      },
      {
        "_doc_count": 1,
        "_key": "5000.0"
      },
      {
        "_doc_count": 4,
        "_key": "6000.0"
      },
      {
        "_doc_count": 12,
        "_key": "7000.0"
      },
      {
        "_doc_count": 7,
        "_key": "8000.0"
      },
      {
        "_doc_count": 1,
        "_key": "9000.0"
      },
      {
        "_doc_count": 9,
        "_key": "10000.0"
      },
      {
        "_doc_count": 1,
        "_key": "12000.0"
      }
    ]
  },
  ...
```

### Shift

By default the bucket keys start with 0 and then continue in even spaced steps
of interval, e.g. if the interval is 10 the first buckets (assuming there is
data inside them) will be `[0, 10)`, `[10, 20)`, `[20, 30)`. The bucket
boundaries can be shifted by using the `_shift` option.

This can be best illustrated with an example. If there are many account holders
with ages ranging from 17 to 44, using interval 10 will result in four buckets:
`[10, 20)`, `[20, 30)`, `[30, 40)`, `[40, 50)`. If an additional `_shift` of 5
is used, however, there will be only three buckets to collect all the account
holders: `[15, 25)`, `[25, 35)`, `[35, 45)`:

{% capture req %}

```json
POST /bank/:search?pretty

{
  "_query": "*",
  "_limit": 0,
  "_check_at_least": 1000,
  "_aggs": {
    "ages": {
      "_histogram": {
        "_field": "age",
        "_interval": 10,
        "_shift": 5
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
    "ages": [
      {
        "_doc_count": 243,
        "_key": "15"
      },
      {
        "_doc_count": 471,
        "_key": "25"
      },
      {
        "_doc_count": 286,
        "_key": "35"
      }
    ]
  },
  ...
}
```

### Ordering

By default, the returned buckets are sorted by their `_key` ascending, though
the order behaviour can be controlled using the `_sort` setting. Supports the
same order functionality as explained in [Bucket Ordering](..#ordering).
