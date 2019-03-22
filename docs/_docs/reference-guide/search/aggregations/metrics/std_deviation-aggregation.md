---
title: Standard Deviation Aggregation
short_title: Standard Deviation
---

A _single-value_ metrics aggregation that computes the standard deviation of
numeric values that are extracted from the aggregated documents.

## Structuring

The following snippet captures the structure of standard deviation aggregations:

```json
"<aggregation_name>": {
  "_std_deviation": {
    "_field": "<field_name>",
    ( "_sigma": <sigma_value> )?
  },
  ...
}
```

### Field

The `<field_name>` in the `_field` parameter defines the specific field from
which the numeric values in the documents are extracted and used to compute the
returned standard deviation.

Assuming the data consists of documents representing bank accounts, as shown in
the sample dataset of [Exploring Your Data]({{ '/docs/exploring' | relative_url }}#sample-dataset)
section:

{% capture req %}

```json
POST /bank/:search?pretty

{
  "_query": "*",
  "_limit": 0,
  "_check_at_least": 1000,
  "_aggs": {
    "balance_std_deviation": {
      "_std_deviation": {
        "_field": "balance"
      }
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

The above aggregation computes the balance standard deviation over all
documents. The above will return the following:


```json
  "aggregations": {
    "_doc_count": 1000,
    "balance_std_deviation": {
      "_std_deviation": 1505.7671709602053,
      "_std_deviation_bounds": {
        "_upper": 5576.56738192041,
        "_lower": -446.50130192041049
      }
    }
  }, ...
```

The name of the aggregation (`balance_std_deviation` above) also serves as the
key by which the aggregation result can be retrieved from the returned response.

### Standard Deviation Bounds

By default, the `_std_deviation` metric will return an object called
`_std_deviation_bounds`, which provides an interval of plus/minus two standard
deviations from the mean. This can be a useful way to visualize variance of your
data. If you want a different boundary, for example three standard deviations,
you can set `_sigma` in the request:

{% capture req %}

```json
POST /bank/:search?pretty

{
  "_query": "*",
  "_limit": 0,
  "_check_at_least": 1000,
  "_aggs": {
    "balance_std_deviation": {
      "_std_deviation": {
        "_field": "balance",
        "_sigma": 3
      }
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

`_sigma` controls how many standard deviations +/- from the mean should be
displayed and can be any non-negative double, meaning you can request
non-integer values such as `1.5`.  A value of `0` is valid, but will simply
return the average for both `_upper` and `_lower` bounds.

{: .note .warning }
**_Standard Deviation and Bounds require normality._**<br>
The standard deviation and its bounds are displayed by default, but they are not
always applicable to all data-sets. Your data must be normally distributed for
the metrics to make sense. The statistics behind standard deviations assumes
normally distributed data, so if your data is skewed heavily left or right, the
value returned will be misleading.
