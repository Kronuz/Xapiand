---
title: Count Aggregation
---

{: .note .construction}
**_TODO:_** This section is a work in progress...

A _single-value_ metrics aggregation that counts the number of values that are
extracted from the aggregated documents. These values are extracted from
specific fields in the documents. Typically, this aggregator will be used in
conjunction with other single-value aggregations. For example, when computing
the `_avg` one might be interested in the number of values the average is
computed over.

Assuming the data consists of documents representing bank accounts, as shown in
the sample dataset of [Exploring Your Data]({{ '/docs/exploring/' | relative_url }}#sample-dataset)
section, computing the number of cities with accounts in the state of Indiana:

{% capture req %}

```json
POST /bank/:search?pretty

{
  "_query": {
    "state": "Indiana"
  },
  "_limit": 0,
  "_check_at_least": 1000,
  "_aggs": {
    "indiana_city_count": {
      "_count": {
        "_field": "city"
      }
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}


Response:

```json
  "#aggregations": {
    "_doc_count": 17,
    "indiana_city_count": {
      "_count": 17
    }
  },
  ...
```
