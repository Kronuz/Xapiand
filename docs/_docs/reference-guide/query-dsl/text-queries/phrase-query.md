---
title: Phrase Query
---

A phrase surrounded with double quotes ("") matches documents containing that exact phrase. Hyphenated words are also treated as phrases, as are cases such as filenames and email addresses (e.g. /etc/passwd or president@whitehouse.gov)

{% capture req %}

```json
GET /bank/:search?pretty

{
  "_query": {
    "personality": "\"these days are few and far between\""
}
```
{% endcapture %}
{% include curl.html req=req %}
