---
title: Values Aggregation
short_title: Values
---

A _multi-bucket_ value source based aggregation where buckets are dynamically
built - one per unique **value**.


## Structuring

The following snippet captures the structure of range aggregations:

```json
"<aggregation_name>": {
  "_values": {
    "_field": "<field_name>"
  },
  ...
}
```

Also supports all other functionality as explained in [Bucket Aggregations](..#structuring).

### Field

The `<field_name>` in the `_field` parameter defines the field on which the
aggregation will act upon.

Assuming the data consists of documents representing bank accounts, as shown in
the sample dataset of [Exploring Your Data]({{ '/docs/exploring' | relative_url }}#sample-dataset)
section, listing all favorite fruits of all account holders:

{% capture req %}

```json
GET /bank/:search

{
  "_query": "*",
  "_limit": 0,
  "_check_at_least": 1000,
  "_aggregations": {
    "favorite_ruits": {
      "_values": {
        "_field": "favoriteFruit"
      }
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

Response:

```json
{
  "aggregations": {
    "_doc_count": 1000,
    "favorite_ruits": [
      {
        "_doc_count": 76,
        "_key": "strawberry"
      },
      {
        "_doc_count": 64,
        "_key": "banana"
      },
      {
        "_doc_count": 89,
        "_key": "apple"
      }
    ]
  }, ...
}
```

### Ordering

By default, the returned buckets are sorted by their `_doc_count` descending,
though the order behaviour can be controlled using the `_sort` setting. Supports
the same order functionality as explained in [Bucket Ordering](..#ordering).
