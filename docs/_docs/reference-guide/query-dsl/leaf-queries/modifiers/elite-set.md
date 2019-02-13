---
title: Elite Set
---

Elite Set

{% capture req %}

```json
GET /bank/:search?pretty

{
  "_query": {
    "personality": {
      "_elite_set": "the biggest two things to know are that hes lovable and cooperative. Of course he's also kind, honest and considerate, but they're far less prominent, especially compared to impulses of being shallow as well"
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}
