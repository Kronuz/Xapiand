---
title: Max Aggregation
short_title: Max
---

A _single-value_ metrics aggregation that keeps track and returns the maximum
value among the numeric values extracted from the aggregated documents. These
values are extracted from specific numeric fields in the documents.


## Structuring

The following snippet captures the structure of max aggregations:

```json
"<aggregation_name>": {
  "_max": {
    "_field": "<field_name>"
  },
  ...
}
```

### Field

The `<field_name>` in the `_field` parameter defines the specific field from
which the numeric values in the documents are extracted and used to compute the
returned maximum value.

Assuming the data consists of documents representing bank accounts, as shown in
the sample dataset of [Data Exploration]({{ '/docs/exploration' | relative_url }}#sample-dataset)
section, computing the max balance value across all accounts:

{% capture req %}

```json
SEARCH /bank/

{
  "_query": "*",
  "_limit": 0,
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
  "aggregations": {
    "_doc_count": 1000,
    "max_balance": {
      "_max": 12699.46
    }
  }, ...
}
```

As can be seen, the name of the aggregation (`max_balance` above) also serves as
the key by which the aggregation result can be retrieved from the returned
response.
