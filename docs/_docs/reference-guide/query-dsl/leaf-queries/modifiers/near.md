---
title: Near Modifier
---

Another commonly used operator is `NEAR`, which finds terms within 10 words
of each other in the current document, behaving like `_and` with regard to
weights, so that:

* Documents which match A within 10 words of B are matched, with weight of A+B

#### Example

{% capture req %}

```json
GET /bank/:search?pretty

{
  "_query": {
    "personality": "adventurous NEAR ambitious"
  }
}
```
{% endcapture %}
{% include curl.html req=req %}


{% capture req %}

```json
GET /bank/:search?pretty

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
