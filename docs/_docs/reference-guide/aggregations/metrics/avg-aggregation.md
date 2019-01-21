---
title: Average Aggregation
---

A _single-value_ metrics aggregation that computes the average of numeric values
that are extracted from the aggregated documents. These values are extracted
from specific numeric fields in the documents.

Assuming the data consists of documents representing bank accounts (as shown in
the sample dataset of [Exploring Your Data]({{ '/docs/exploring/' | relative_url }}#sample-dataset) section:

{% capture req %}

```json
POST /bank/:search?pretty

{
  "_query": "*",
  "_limit": 0,
  "_check_at_least": 1000,
  "_aggs": {
    "avg_age": {
      "_avg": {
        "_field": "age"
      }
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

The above aggregation computes the average grade over all documents. The
aggregation type is `_avg` and the `_field` setting defines the numeric field
of the documents the average will be computed on. The above will return the
following:

```json
{
    "#aggregations": {
        "_doc_count": 1000,
        "avg_age": {
            "_avg": 30.034
        }
    },
    ...
}
```

The name of the aggregation (`avg_grade` above) also serves as the key by which
the aggregation result can be retrieved from the returned response.


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
    "avg_balance": {
      "_avg": {
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
