---
title: Extended Statistics Aggregation
short_title: Extended Statistics
---

A _multi-value_ metrics aggregation that computes extended statistics over
numeric values extracted from the aggregated documents.

The `_extended_stats` aggregations is an extended version of the
[Statistics Aggregation](stats-aggregation), where additional metrics are added
such as `_sum_of_squares`, `_variance`, `_std_deviation` and
`_std_deviation_bounds`, these are detailed in their corresponding aggregation
section:

* [Variance Aggregation](variance-aggregation)
* [Standard Deviation Aggregation](std_deviation-aggregation)


## Structuring

The following snippet captures the structure of extended statistics aggregations:

```json
"<aggregation_name>": {
  "_extended_stats": {
    "_field": "<field_name>"
  },
  ...
}
```

### Field

The `<field_name>` in the `_field` parameter defines the specific field from
which the numeric values in the documents are extracted and used to compute the
returned extended statistics.

Assuming the data consists of documents representing bank accounts, as shown in
the sample dataset of [Exploring Your Data]({{ '/docs/exploring' | relative_url }}#sample-dataset)
section:

{% capture req %}

```json
SEARCH /bank/

{
  "_query": "*",
  "_limit": 0,
  "_check_at_least": 1000,
  "_aggs": {
    "balance_stats": {
      "_extended_stats": {
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
      "_sum": 2565033.04,
      "_sum_of_squares": 8844461934.66,
      "_variance": 2267334.7731415,
      "_std_deviation": 1505.7671709602053,
      "_std_deviation_bounds": {
        "_upper": 5576.56738192041,
        "_lower": -446.50130192041049
      }
    }
  }, ...
```

The name of the aggregation (`balance_stats` above) also serves as the key by
which the aggregation result can be retrieved from the returned response.
