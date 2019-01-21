---
title: Histogram Aggregation
---

A _multi-bucket_ values source based aggregation that can be applied on numeric
values extracted from the documents. It dynamically builds fixed size
(a.k.a. interval) buckets over the values. For example, if the documents have a
field that holds a balance (numeric), we can configure this aggregation to
dynamically build buckets with interval `500` (in case of balance it may
represent $500). When the aggregation executes, the balance field of every
document will be evaluated and will be rounded down to its closest bucket - for
example, if the balance is `3200` and the bucket size is `500`
then the rounding will yield `3000` and thus the document will "fall" into the
bucket that is associated with the key `3000`. To make this more formal, here is
the rounding function that is used:

```cpp
bucket_key = floor((value - offset) / interval) * interval + offset;
```

The `interval` must be a positive decimal, while the `offset` must be a decimal
in `[0, interval)` (a decimal greater than or equal to `0` and less than
`interval`)

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
        "_interval": 500
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
    "balances": {
      "1000.0": {
        "_doc_count": 153
      },
      "1500.0": {
        "_doc_count": 165
      },
      "2000.0": {
        "_doc_count": 167
      },
      "2500.0": {
        "_doc_count": 174
      },
      "3000.0": {
        "_doc_count": 179
      },
      "3500.0": {
        "_doc_count": 162
      }
    }
  },
  ...
```


### Minimum document count

{: .note .unreleased}
**_Unimplemented Feature!_**<br>
This feature hasn't yet been implemented...
[Pull requests are welcome!]({{ site.repository }}/pulls)

The response above show that no buckets for balances that fall within the ranges
of `[0, 500)` and `[500, 1000)`, because there are none. By default the response
will not fill  gaps in the histogram. It is possible change that and request
buckets with a minimum count by using the `_min_doc_count` setting:

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
        "_interval": 500,
        "_min_doc_count": 0
      }
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

Response:

```json
  "#aggregations": {
    "_doc_count": 1000,
    "balances": {
      "0.0": {
        "_doc_count": 0
      },
      "500.0": {
        "_doc_count": 0
      },
      "1000.0": {
        "_doc_count": 153
      },
      "1500.0": {
        "_doc_count": 165
      },
      "2000.0": {
        "_doc_count": 167
      },
      "2500.0": {
        "_doc_count": 174
      },
      "3000.0": {
        "_doc_count": 179
      },
      "3500.0": {
        "_doc_count": 162
      }
    }
  },
  ...
```
