---
title: Max Aggregation
---

A _single-value_ metrics aggregation that keeps track and returns the maximum
value among the numeric values extracted from the aggregated documents. These
values are extracted from specific numeric fields in the documents.

{: .note .info}
**_Limits_**<br>
The `_max` aggregation operates on the `double` representation of the data.
As a consequence, the result may be approximate when running on longs whose
absolute value is greater than 2^53.

Computing the max balance value across all accounts:

{% capture req %}

```json
POST /bank/:search?pretty

{
  "_query": "*",
  "_limit": 0,
  "_check_at_least": 1000,
  "_aggs": {
    "max_balance": {
      "_max": {
        "_field": "balance"
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
        "max_balance": {
            "_max": 3998.71
        }
    },
    ...
}
```

As can be seen, the name of the aggregation (`max_price` above) also serves as
the key by which the aggregation result can be retrieved from the returned
response.


## Missing value

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
    "max_balance": {
      "_max": {
        "_field": "balance",
        "_missing": 0
      }
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

Documents without a value in the `balance` field will fall into the same bucket
as documents that have the value `0`.