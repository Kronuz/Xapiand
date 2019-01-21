---
title: Extended Statistics Aggregation
---

A _multi-value_ metrics aggregation that computes stats over numeric values
extracted from the aggregated documents. These values are extracted from
specific numeric fields in the documents.

The `_extended_stats` aggregations is an extended version of the
[Statistics Aggregation](stats), where additional metrics are added such as
`_sum_of_squares`, `_variance`, `_std_deviation` and `_std_deviation_bounds`.

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
aggregation type is `_extended_stats` and the `_field` setting defines the
numeric field of the documents the stats will be computed on. The above will
return the following:


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
      	"_upper": 3000,
      	"_lower": 500
      }
    }
  },
  ...
```

The name of the aggregation (`balance_stats` above) also serves as the key by
which the aggregation result can be retrieved from the returned response.

### Standard Deviation Bounds

{: .note .unreleased}
**_Unimplemented Feature!_**<br>
This feature hasn't yet been implemented...
[Pull requests are welcome!]({{ site.repository }}/pulls)

By default, the `_extended_stats` metric will return an object called
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
    "balance_stats": {
      "_extended_stats": {
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

{: .note .warning}
**_Standard Deviation and Bounds require normality._**<br>
The standard deviation and its bounds are displayed by default, but they are not
always applicable to all data-sets. Your data must be normally distributed for
the metrics to make sense. The statistics behind standard deviations assumes
normally distributed data, so if your data is skewed heavily left or right, the
value returned will be misleading.
