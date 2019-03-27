---
title: Near Query
short_title: Near
---

Another commonly used operator is `_near`, which finds terms within 10 words
of each other in the current document, behaving like `_and` with regard to
weights, so that:

* Documents which match A within 10 words of B are matched, with weight of A+B

#### Example

{% capture req %}

```json
SEARCH /bank/

{
  "_query": {
    "personality": {
      "_near": "adventurous ambitious"
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}
