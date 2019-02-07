---
title: Near Query
---

The `_near` matches documents containing those words within 10 words of each other.


{% capture req %}

```json
GET /bank/:search?pretty

{
  "_query": {
    "_near": [
      {"personality": "biggest"},
      {"personality": "lovable"},
      {"personality": "honest"}
    ]
  }
}
```
{% endcapture %}
{% include curl.html req=req %}
