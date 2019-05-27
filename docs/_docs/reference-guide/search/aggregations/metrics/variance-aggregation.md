---
title: Variance Aggregation
short_title: Variance
---

A _single-value_ metrics aggregation that computes the variance of
numeric values that are extracted from the aggregated documents.

## Structuring

The following snippet captures the structure of variance aggregations:

```json
"<aggregation_name>": {
  "_variance": {
    "_field": "<field_name>"
  },
  ...
}
```

### Field

The `<field_name>` in the `_field` parameter defines the specific field from
which the numeric values in the documents are extracted and used to compute the
returned variance.

Assuming the data consists of documents representing bank accounts, as shown in
the sample dataset of [Data Exploration]({{ '/docs/exploration' | relative_url }}#sample-dataset)
section:

{% capture req %}

```json
SEARCH /bank/

{
  "_query": "*",
  "_limit": 0,
  "_check_at_least": 1000,
  "_aggs": {
    "balance_variance": {
      "_variance": {
        "_field": "balance"
      }
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

The above aggregation computes the balance variance over all documents. The
above will return the following:

2267334.7731415

```json
  "aggregations": {
    "_doc_count": 1000,
    "balance_variance": {
      "_variance": 2267334.7731415
    }
  }, ...
```

The name of the aggregation (`balance_variance` above) also serves as the
key by which the aggregation result can be retrieved from the returned response.
