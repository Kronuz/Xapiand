---
title: Min Aggregation
---

A _single-value_ metrics aggregation that keeps track and returns the minimum
value among numeric values extracted from the aggregated documents. These
values are extracted from specific numeric fields in the documents.


## Structuring

The following snippet captures the structure of min aggregations:

```json
"<aggregation_name>": {
  "_min": {
      "_field": "<field_name>"
  },
  ...
}
```

### Field

The `<field_name>` in the `_field` parameter defines the specific field from
which the numeric values in the documents are extracted and used to compute the
returned minimum value.

Assuming the data consists of documents representing bank accounts, as shown in
the sample dataset of [Exploring Your Data]({{ '/docs/exploring/' | relative_url }}#sample-dataset)
section, computing the min balance value across all accounts:

{% capture req %}

```json
POST /bank/:search?pretty

{
  "_query": "*",
  "_limit": 0,
  "_check_at_least": 1000,
  "_aggs": {
    "min_balance": {
      "_min": {
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
    "min_balance": {
      "_min": 1002.25
    }
  },
  ...
}
```

As can be seen, the name of the aggregation (`min_balance` above) also serves as
the key by which the aggregation result can be retrieved from the returned
response.
