---
title: Simple Queries
---

The most basic query is a search for a single field value. This will find all
documents in the database which have that value assigned to them (be it text,
numeric, or of any other datatype).

#### Example

For example, a search might be for the value "_banana_" assigned to the
"_favoriteFruit_" field and restricting the size of results to `1` result by
using the keyword `_limit`, which by default is set to `10`:

{% capture req %}

```json
GET /bank/:search?pretty

{
  "_query": {
    "favoriteFruit": "banana"
  },
  "_limit": 1
}
```
{% endcapture %}
{% include curl.html req=req %}

When a query is executed, the result is a list of documents that match the
query, together with a **weight**, a **rank** and a **percent** for each which
indicates how good a match for the query that particular document is.
