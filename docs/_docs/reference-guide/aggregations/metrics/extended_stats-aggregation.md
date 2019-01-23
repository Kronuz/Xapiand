---
title: Extended Statistics Aggregation
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

#### Field

The `<field_name>` in the `_field` parameter defines the specific field from
which the numeric values in the documents are extracted and used to compute the
returned extended statistics.

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
  "#aggregations": {
    "_doc_count": 1000,
    "balance_stats": {
      "_count": 1000,
      "_min": 1002.25,
      "_max": 3998.71,
      "_avg": 2522.83304,
      "_sum": 2522833.04,
      "_sum_of_squares": 7085869104.66,
      "_variance": 721904.4614057642,
      "_std_deviation": 849.649610960756,
      "_std_deviation_bounds": {
        "_upper": 4222.1322619215129,
        "_lower": 823.5338180784878
      }
    }
  },
  ...
```

The name of the aggregation (`balance_stats` above) also serves as the key by
which the aggregation result can be retrieved from the returned response.
