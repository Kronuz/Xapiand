---
title: Sum Aggregation
---

A _single-value_ metrics aggregation that sums up numeric values that are
extracted from the aggregated documents. These values are extracted from
specific numeric fields in the documents.

Assuming the data consists of documents representing bank accounts, as shown in
the sample dataset of [Exploring Your Data]({{ '/docs/exploring/' | relative_url }}#sample-dataset)
section, we can sum the balances of all accounts in the state of Indiana with:

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
    "indiana_total_balance": {
      "_sum": {
        "_field": "balance"
      }
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

Resulting in:

```json
{
    "#aggregations": {
        "_doc_count": 17,
        "indiana_total_balance": {
            "_sum": 45152.87
        }
    },
    ...
}
```


## Missing value

{: .note .unreleased}
**_Unimplemented Feature!_**<br>
This feature hasn't yet been implemented...
[Pull requests are welcome!]({{ site.repository }}/pulls)

The `_missing` parameter defines how documents that are missing a value should
be treated. By default documents missing the value will be ignored but it is
also possible to treat them as if they had a value. For example, this treats
all Indiana accounts without a balance as being `1000.0`.

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
    "indiana_total_balance": {
      "_sum": {
        "_field": "balance",
        "_missing": 1000.0
      }
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}
