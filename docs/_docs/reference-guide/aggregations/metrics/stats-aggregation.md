---
title: Statistics Aggregation
---

A _multi-value_ metrics aggregation that computes stats over numeric values
extracted from the aggregated documents. These values are extracted from
specific numeric fields in the documents.

The stats that are returned consist of: `_min`, `_max`, `_sum`, `_count` and `_avg`.

Assuming the data consists of documents representing bank accounts, as shown in
the sample dataset of [Exploring Your Data]({{ '/docs/exploring/' | relative_url }}#sample-dataset)
section:

{% capture req %}

```json
POST /bank/:search?pretty

{
  "_query": "*",
  "_limit": 0,
  "_check_at_least": 1000,
  "_aggs": {
    "balance_stats": {
      "_stats": {
        "_field": "balance"
      }
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

The above aggregation computes the balance statistics over all documents. The
aggregation type is `_stats` and the `_field` setting defines the numeric field
of the documents the stats will be computed on. The above will return the
following:


```json
  "#aggregations": {
    "_doc_count": 1000,
    "balance_stats": {
      "_count": 1000,
      "_min": 1002.25,
      "_max": 3998.71,
      "_avg": 2522.83304,
      "_sum": 2522833.04
    }
  },
  ...
```
