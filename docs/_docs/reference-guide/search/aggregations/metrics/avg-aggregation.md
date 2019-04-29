---
title: Average Aggregation
short_title: Average
---

A _single-value_ metrics aggregation that computes the average of numeric values
that are extracted from the aggregated documents.

## Structuring

The following snippet captures the structure of average aggregations:

```json
"<aggregation_name>": {
  "_avg": {
    "_field": "<field_name>"
  },
  ...
}
```

### Field

The `<field_name>` in the `_field` parameter defines the specific field from
which the numeric values in the documents are extracted and used to compute the
returned average.

Assuming the data consists of documents representing bank accounts, as shown in
the sample dataset of [Data Exploration]({{ '/docs/exploration' | relative_url }}#sample-dataset)
section, computing the average age of all account holders:

{% capture req %}

```json
SEARCH /bank/

{
  "_query": "*",
  "_limit": 0,
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

The above aggregation computes the average grade over all documents. The above
will return the following:

```json
{
  "aggregations": {
    "_doc_count": 1000,
    "avg_age": {
      "_avg": 30.042
    }
  }, ...
}
```

The name of the aggregation (`avg_grade` above) also serves as the key by which
the aggregation result can be retrieved from the returned response.
