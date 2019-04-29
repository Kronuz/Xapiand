---
title: Statistics Aggregation
short_title: Statistics
---

A _multi-value_ metrics aggregation that computes statistics over numeric values
extracted from the aggregated documents.

The stats that are returned consist of `_min`, `_max`, `_sum`, `_count` and
`_avg`, each of which is an aggregation by itself and is detailed in it's own
aggregation section:

* [Min Aggregation](min-aggregation)
* [Max Aggregation](max-aggregation)
* [Sum Aggregation](sum-aggregation)
* [Count Aggregation](count-aggregation)


## Structuring

The following snippet captures the structure of statistics aggregations:

```json
"<aggregation_name>": {
  "_stats": {
    "_field": "<field_name>"
  },
  ...
}
```

### Field

The `<field_name>` in the `_field` parameter defines the specific field from
which the numeric values in the documents are extracted and used to compute the
returned statistics.

Assuming the data consists of documents representing bank accounts, as shown in
the sample dataset of [Data Exploration]({{ '/docs/exploration' | relative_url }}#sample-dataset)
section:

{% capture req %}

```json
SEARCH /bank/

{
  "_query": "*",
  "_limit": 0,
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
above will return the following:


```json
  "aggregations": {
    "_doc_count": 1000,
    "balance_stats": {
      "_count": 1000,
      "_min": 7.99,
      "_max": 12699.46,
      "_avg": 2565.03304,
      "_sum": 2565033.04
    }
  }, ...
```
