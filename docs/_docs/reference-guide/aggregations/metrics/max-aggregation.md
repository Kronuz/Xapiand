---
title: Max Aggregation
---

A _single-value_ metrics aggregation that keeps track and returns the maximum
value among the numeric values extracted from the aggregated documents. These
values are extracted from specific numeric fields in the documents.

{: .note .info}
**_Limits_**<br>
The `_min` and `_max` aggregation operate on the `double` representation of the
data. As a consequence, the result may be approximate when running on longs
whose absolute value is greater than 2^53.

Computing the max price value across all documents

{% capture req %}

```json
POST /exams/:search?pretty

{
  "_query": "*",
  "_limit": 0,
  "_check_at_least": 1000,
  "_aggs": {
    "max_price": {
      "_max": {
        "_field": "price"
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
    ...
    "#aggregations": {
        "max_price": {
            "value": 200.0
        }
    }
}
```

As can be seen, the name of the aggregation (`max_price` above) also serves as
the key by which the aggregation result can be retrieved from the returned
response.
