---
title: Default Operator
---

The default operator for text queries can be set by using the desired modifier.
Available modifiers are `_or` (the default), `_elite_set` and `_and`.

### Example

To make **AND** the default operator and thus forcing a query to search for
**all** terms instead of _any_ term (which is the default):

{% capture req %}

```json
GET /bank/:search?pretty

{
  "_query": {
    "personality": {
      "_and": "these days are few and far between"
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}
