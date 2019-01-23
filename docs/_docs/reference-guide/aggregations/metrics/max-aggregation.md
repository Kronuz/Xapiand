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

Assuming the data consists of documents representing bank accounts, as shown in
the sample dataset of [Exploring Your Data]({{ '/docs/exploring/' | relative_url }}#sample-dataset)
section, computing the max balance value across all accounts:

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
