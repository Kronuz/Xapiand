---
title: Filter Aggregation
---

Defines a _single-bucket_ of all the documents in the current document set
context that match a specified filter.

## Structuring

The following snippet captures the structure of filter aggregations:

```json
"<aggregation_name>": {
  "_filter": {
      "_term": {
        ( "<key>": <value>, )*
      }
  },
  ...
}
```

Also supports all other functionality as explained in [Bucket Aggregations](..#structuring).

### Filtering Terms

Often this will be used to narrow down the current aggregation context to a
specific set of documents containing certain terms.

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
    "strawberry_lovers": {
      "_filter": {
        "_term": {
          "favoriteFruit": "strawberry"
        }
      },
      "_aggs": {
        "avg_balance": {
          "_avg": {
            "_field": "balance"
          }
        }
      }
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

In the above example, we calculate the average balance of all the bank accounts
with holders who fancy strawberries.

Response:

```json
{
  "#aggregations": {
    "_doc_count": 1000,
    "strawberry_lovers": {
      "_doc_count": 345,
      "avg_balance": {
        "_avg": 2576.7695072463768
      }
    }
  },
}
```
