---
title: Terms Aggregation
short_title: Terms
---

{: .note .construction }
_This section is a **work in progress**..._


A _multi-bucket_ value source based aggregation where buckets are dynamically
built - one per unique **term**.

{: .note .caution }
**_Performance_**<br>
Whenever is possible, prefer [Values Aggregation](../values-aggregation) to this type as
it's more efficient.


## Structuring

The following snippet captures the structure of range aggregations:

```json
"<aggregation_name>": {
  "_terms": {
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
section:

{% capture req %}

```json
GET /bank/:search?pretty

{
  "_query": "*",
  "_limit": 0,
  "_check_at_least": 1000,
  "_aggregations": {
    "most_used_terms": {
      "_terms": {
        "_field": "personality"
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
    "most_used_terms": [
      ...
    ]
  }, ...
}
```

### Ordering

By default, the returned buckets are sorted by their `_doc_count` descending,
though the order behaviour can be controlled using the `_sort` setting. Supports
the same order functionality as explained in [Bucket Ordering](..#ordering).
